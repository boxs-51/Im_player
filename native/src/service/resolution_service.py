from flask import Flask, request, jsonify
from yt_dlp import YoutubeDL
import threading
import time
from concurrent.futures import ThreadPoolExecutor, TimeoutError
from urllib.parse import urlparse, parse_qs, urlencode, urlunparse
import os
from cachetools import TTLCache
import logging

from cookies import detect_and_save_cookies  # đảm bảo file này còn tồn tại

app = Flask(__name__)

# ------------------- CONFIG -------------------
cookies_path_file = r"D:\MPVPLAYER\imgui_player\temp\cookies.txt"
executor = ThreadPoolExecutor(max_workers=10)
_cache = TTLCache(maxsize=1000, ttl=24*3600)
_cache_lock = threading.Lock()
cookie_refresh_attempted = False
cookie_refresh_lock = threading.Lock()

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)
# ----------------------------------------------

# ------------------- HELPERS ------------------

def is_local_file(url):
    return os.path.isfile(url)

def is_url(path_or_url):
    parsed = urlparse(path_or_url)
    return parsed.scheme in ("http", "https")

def classify_youtube_url(url):
    parsed = urlparse(url)
    query = parse_qs(parsed.query)
    if "v" in query and "list" not in query:
        return "video"
    elif "list" in query:
        # Kiểm tra xem list có phải radio/mix không
        list_id = query["list"][0]
        if list_id.startswith("RD") or list_id.startswith("PL"):  # RD = Radio, PL = Playlist
            return "playlist_or_radio"
        return "playlist"
    else:
        return "unknown"

def detect_type_by_path_or_url(path_or_url):
    if is_local_file(path_or_url):
        return "file"
    elif is_url(path_or_url):
        yt_type = classify_youtube_url(path_or_url)
        if yt_type != "unknown":
            return yt_type
        else:
            return "url"  # phải dùng yt-dlp check
    else:
        return "unknown"
    
def clean_youtube_url(url):
    parsed = urlparse(url)
    query = parse_qs(parsed.query)
    video_id = query.get("v")
    if not video_id:
        return url
    new_query = urlencode({"v": video_id[0]})
    return urlunparse((parsed.scheme, parsed.netloc, parsed.path, parsed.params, new_query, parsed.fragment))

def check_cookie_valid(cookie_path):
    try:
        with open(cookie_path, "r", encoding="utf-8") as f:
            content = f.read()
    except FileNotFoundError:
        logger.warning(f"[COOKIE] Cookie file not found: {cookie_path}")
        return False

    required_tokens = ["SID", "HSID", "SSID", "SAPISID", "APISID"]
    valid = all(token in content for token in required_tokens)
    if not valid:
        logger.warning("[COOKIE] Cookie may not allow full HD / private / age-restricted videos.")
    return valid
# ----------------------------------------------

# ------------------- VIDEO DETECTION ------------------
def detect_video_type_ydlp(url, cookies_path_file=None):
    ydl_opts = {
        'quiet': True,
        'skip_download': True,
        'nocheckcertificate': True,
        'extract_flat': 'in_playlist',
    }
    if cookies_path_file and check_cookie_valid(cookies_path_file):
        ydl_opts['cookiefile'] = cookies_path_file

    with YoutubeDL(ydl_opts) as ydl:
        try:
            info = ydl.extract_info(url, download=False)
        except Exception:
            return {"type": "video", "video_id": None, "note": "fallback: không lấy được chi tiết, chỉ public info"}

    if info.get("_type") == "playlist":
        video_ids = [entry.get("id") for entry in info.get("entries", []) if entry.get("id")]
        return {"type": "playlist", "playlist_id": info.get("id"), "video_ids": video_ids}
    elif info.get("is_live"):
        return {"type": "livestream", "video_id": info.get("id")}
    elif info.get("_type") in ["url", "video"]:
        return {"type": "video", "video_id": info.get("id")}
    else:
        return {"type": "video", "video_id": None, "note": "fallback: không xác định, chỉ public info"}

def ensure_cookie_valid():
    """Kiểm tra cookie hiện tại, trả về True nếu hợp lệ, False nếu không"""
    if check_cookie_valid(cookies_path_file):
        logger.info("[COOKIE] Cookie hiện tại hợp lệ, dùng luôn")
        return True
    else:
        logger.info("[COOKIE] Cookie hiện tại không hợp lệ")
        return False
    
def detect_video_type_and_ids(url):
    if is_local_file(url):
        return {"type": "file", "path": url}

    if is_url(url):
        yt_type = classify_youtube_url(url)
        if yt_type in ["video", "playlist", "playlist_or_radio"]:
            try:
                info = detect_video_type_ydlp(url, cookies_path_file=cookies_path_file)
                # nếu radio, ép type thành "video"
                if yt_type == "playlist_or_radio":
                    info["type"] = "video"
                    if "video_ids" in info and info["video_ids"]:
                        info["video_id"] = info["video_ids"][0]
                        info.pop("video_ids", None)
                return info
            except Exception:
                return {"type": "video", "video_id": None, "note": "fallback: không lấy được chi tiết, chỉ public info"}
        else:
            return {"type": "video", "video_id": None, "note": "fallback: không lấy được chi tiết, chỉ public info"}

    return {"type": "unknown"}

def get_or_fetch_resolutions(url):
    clean_url = clean_youtube_url(url)
    with _cache_lock:
        if clean_url in _cache:
            return _cache[clean_url]

    result = fetch_resolutions(clean_url)
    with _cache_lock:
        _cache[clean_url] = result
    return result
# --------------------------------------------------------

# ------------------- FETCH RESOLUTIONS ------------------
def fetch_resolutions(url):
    global cookie_refresh_attempted
    cookie_valid = check_cookie_valid(cookies_path_file)

    ydl_opts = {
        'quiet': True,
        'no_warnings': True,
        'listformats': True,
        'extract_flat': True, 
        'skip_download': True,
        'nocheckcertificate': True,
        'cachedir': False,
        'extractor_args': {'youtube': 'skip=hls,dash'},
        'player_skip': 'webpage, js, client',
    }

    if cookie_valid:
        ydl_opts['cookiefile'] = cookies_path_file

    try:
        with YoutubeDL(ydl_opts) as ydl:
            info = ydl.extract_info(url, download=False)
            formats = info.get('formats', [])
            results = []
            for f in formats:
                h = f.get('height')
                vcodec = f.get('vcodec')
                acodec = f.get('acodec')
                if h and vcodec:
                    results.append({"resolution": f"{h}p", "vcodec": vcodec, "acodec": acodec})
            # loại bỏ trùng lặp
            seen = set()
            unique = []
            for r in results:
                key = (r["resolution"], r["vcodec"])
                if key not in seen:
                    seen.add(key)
                    unique.append(r)
            unique.sort(key=lambda x: int(x["resolution"][:-1]))
            return {"formats": unique, "cookie_valid": cookie_valid}
    except Exception as e:
        logger.warning(f"fetch_resolutions lỗi: {e}")
        if cookie_valid:
            logger.info("Thử fallback: bỏ cookie và fetch lại public info...")
            return fetch_resolutions_without_cookie(url)
        else:
            # fallback trả về unknown / restricted
            return {"formats": [], "cookie_valid": False, "note": "fallback: không lấy được thông tin đầy đủ"}
        
def fetch_resolutions_without_cookie(url):
    ydl_opts = {
        'quiet': True,
        'no_warnings': True,
        'listformats': True,
        'skip_download': True,
        'nocheckcertificate': True,
        'cachedir': False,
    }
    try:
        with YoutubeDL(ydl_opts) as ydl:
            info = ydl.extract_info(url, download=False)
            formats = info.get('formats', [])
            results = []
            for f in formats:
                h = f.get('height')
                vcodec = f.get('vcodec')
                acodec = f.get('acodec')
                if h and vcodec:
                    results.append({"resolution": f"{h}p", "vcodec": vcodec, "acodec": acodec})
            return {"formats": results, "cookie_valid": False, "note": "fallback, chỉ lấy public formats"}
    except Exception as e:
        logger.warning(f"fetch_resolutions_without_cookie lỗi: {e}")
        return {"formats": [], "cookie_valid": False, "note": "không lấy được thông tin"}

def fetch_and_cache_video(url):
    clean_url = clean_youtube_url(url)
    try:
        res = fetch_resolutions(clean_url)
        with _cache_lock:
            _cache[clean_url] = res
    except Exception:
        with _cache_lock:
            _cache[clean_url] = ["error"]
# --------------------------------------------------------

# ------------------- PLAYLIST ------------------
def get_playlist_first_inner(video_ids):
    if not video_ids:
        return jsonify({"error": "Empty playlist"}), 400

    video_urls = [f"https://www.youtube.com/watch?v={vid}" for vid in video_ids]
    first_url = video_urls[0]
    with _cache_lock:
        first_res = _cache.get(first_url)
    if not first_res:
        try:
            first_res = fetch_resolutions(first_url)
            with _cache_lock:
                _cache[first_url] = first_res
        except Exception:
            first_res = ["error"]

    response = {
        "playlist_id": None,
        "video_type": "playlist",
        "videos": [{"index": 1, "video": first_url, "resolutions": first_res}]
    }

    def background_fetch_rest():
        for idx, url in enumerate(video_urls[1:], start=2):
            with _cache_lock:
                if url in _cache:
                    continue
            fetch_and_cache_video(url)

    threading.Thread(target=background_fetch_rest, daemon=True).start()
    return jsonify(response)
# --------------------------------------------------------

# ------------------- API ENDPOINT ------------------
@app.route('/get_resolutions_auto', methods=['POST'])
def get_resolutions_auto():
    data = request.get_json()
    if not data or "url" not in data:
        return jsonify({"error": "Missing URL"}), 400

    url = data["url"]
    global cookie_refresh_attempted
    cookie_refresh_attempted = False

    info = detect_video_type_and_ids(url)

    if info["type"] == "file":
        return jsonify({"video": info["path"], "video_type": "file", "resolutions": ["local"]})
    elif info["type"] in ["playlist", "playlist_or_radio"]:
        # Nếu là radio, chỉ lấy video đầu tiên
        return get_playlist_first_inner(info["video_ids"])
    elif info["type"] == "livestream":
        return jsonify({"video": url, "video_type": "livestream", "resolutions": ["live"]})
    elif info["type"] == "video":
        result = get_or_fetch_resolutions(url)
        return jsonify({
            "video": clean_youtube_url(url),
            "video_type": "video",
            "resolutions": result.get("formats", []),
            "cookie_valid": result.get("cookie_valid", False)
        })
    else:
        return jsonify({"error": "Unknown video type"}), 400
    
@app.route('/retry_cookie', methods=['POST'])
def retry_cookie():
    try:
        logger.info("Người dùng yêu cầu lấy cookie mới...")
        detect_and_save_cookies(cookies_path_file)
        valid = check_cookie_valid(cookies_path_file)
        return jsonify({
            "success": valid,
            "message": "Cookie mới hợp lệ" if valid else "Cookie mới không hợp lệ"
        })
    except Exception as e:
        logger.warning(f"Lỗi khi lấy cookie: {e}")
        return jsonify({"success": False, "message": str(e)}), 500
# --------------------------------------------------------

# ------------------- COOKIE BACKGROUND ------------------
def background_cookie_fetch_once():
    try:
        logger.info("Khởi chạy lấy cookie nền lúc start app...")
        detect_and_save_cookies(cookies_path_file)
        logger.info("Lấy cookie nền hoàn tất.")
    except Exception as e:
        logger.warning("Lỗi khi lấy cookie :", e)
# --------------------------------------------------------

if __name__ == "__main__":
    t = threading.Thread(target=background_cookie_fetch_once, daemon=True)
    t.start()
    app.run(port=5005, debug=True, use_reloader=False)
