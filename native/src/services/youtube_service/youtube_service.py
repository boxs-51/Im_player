import unicodedata
from fastapi import FastAPI, Response , HTTPException ,Query
from fastapi.middleware.cors import CORSMiddleware
from typing import Optional , List
import requests, random, os
from cachetools import TTLCache
from collections import deque
import re
from urllib.parse import unquote , parse_qs ,urlparse
import json
import time
from pydantic import BaseModel
import math
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading

KEY_SERVER = "http://127.0.0.1:6000"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# Thư mục gốc của app (cha của src/)
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
# File dữ liệu (nằm ở D:\MPVPLAYER\imgui_player\data\search_keywords.json)
KEYS_FILE = os.path.join(ROOT_DIR, "data", "search_keywords.json")
# ==================== API KEYS ====================
LOCAL_KEYS = [
]
local_index = 0
OFFLINE_MODE = False 
MAX_INTERACTIONS= 500
DEFAULT_EXPIRE_SECONDS = 7*24*3600  # 7 ngày

# ==================== APP ====================
app = FastAPI()
app.add_middleware(
    CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"]
)

# ==================== RECENT VIDEOS ====================
recent_videos = []  # Lưu ID video gần đây để tránh lặp
class VideoItem(BaseModel):
    id: str
    title: str
    link: str
# ==================== CACHE ====================
search_cache = TTLCache(maxsize=1000, ttl=24*3600)
trending_cache = TTLCache(maxsize=1000, ttl=24*3600)

search_keywords = {
    "keyword_normalized": {
        "count": 5,         # số lần tìm kiếm
        "last_seen": 1690000000,  # timestamp lần cuối xuất hiện
        "expire_seconds": 7*24*3600,  # khoảng thời gian tồn tại mặc định
        "auto_remove_if_rementioned": True  # nếu được nhắc lại sẽ tự loại
    }
}
video_key = "some_unique_key"
search_keywords[video_key] = {
    "count": 1,
    "mention_count": 0,
    "last_seen": time.time(),
    "expire_seconds": DEFAULT_EXPIRE_SECONDS,
    "is_representative": True   # cờ key đại diện
}
cache_locks = {}
executor = ThreadPoolExecutor(max_workers=5)
# ==================== HELPER FUNCTIONS ====================
def acquire():
    global local_index
    try:
        res = requests.post(f"{KEY_SERVER}/acquire", timeout=3)
        res.raise_for_status()
        return res.json()
    except Exception:
        # fallback dùng key local
        key = LOCAL_KEYS[local_index]
        local_index = (local_index + 1) % len(LOCAL_KEYS)
        return {"api_key": key, "fallback": True}

def release(key):
    requests.post(f"{KEY_SERVER}/release", json={"api_key": key})

def report_quota_exceeded(key):
    requests.post(f"{KEY_SERVER}/report", json={"api_key": key, "error": "quota exceeded"})

def get_lock(key):
    if key not in cache_locks:
        cache_locks[key] = threading.Lock()
    return cache_locks[key]

def prune_keywords():
    now = time.time()
    keys_to_delete = [
        k for k, v in search_keywords.items()
        if v.get("expire_seconds") is not None and now - v.get("last_seen",0) > v.get("expire_seconds", DEFAULT_EXPIRE_SECONDS)
    ]
    for k in keys_to_delete:
        del search_keywords[k]

    if keys_to_delete:
        save_search_keywords()

def load_search_keywords():
    global search_keywords
    try:
        with open(KEYS_FILE, "r", encoding="utf-8") as f:
            search_keywords = json.load(f)
    except FileNotFoundError:
        search_keywords = {}


def save_search_keywords():
    with open(KEYS_FILE, "w", encoding="utf-8") as f:
        json.dump(search_keywords, f, ensure_ascii=False, indent=2)

def track_search_keyword(keyword):
    key = normalize_text(keyword)
    now = time.time()

    if key in search_keywords:
        entry = search_keywords[key]
        # Tăng số lần nhắc (mở video)
        entry["mention_count"] = entry.get("mention_count", 0) + 1
        entry["count"] += 1
        entry["last_seen"] = now

        # Nếu đạt ngưỡng tương tác → key tồn tại vĩnh viễn, không còn giới hạn
        if entry["mention_count"] >= MAX_INTERACTIONS:
            entry["expire_seconds"] = None  # None = không bao giờ bị prune
    else:
        # Key mới
        search_keywords[key] = {
            "count": 1,
            "mention_count": 1,
            "last_seen": now,
            "expire_seconds": DEFAULT_EXPIRE_SECONDS,
            "is_representative": False
        }

    prune_keywords()  # Loại từ khóa hết hạn
    save_search_keywords()

def top_search_keywords(n=5):
    # prune trước khi lấy top
    prune_keywords()
    return sorted(
        [(k, v["count"]) for k,v in search_keywords.items()],
        key=lambda x: -x[1]
    )[:n]
def check_network():
    global OFFLINE_MODE
    try:
        # Ping nhanh Google DNS
        requests.get("https://www.google.com", timeout=3)
        OFFLINE_MODE = False
        return True
    except requests.RequestException:
        OFFLINE_MODE = True
        return False


def refill_cache_background(cache, key, fetch_func,page_token=None, batch_size=5, refill_threshold=10):
    """
    Refill cache: nếu key chưa có -> fetch trực tiếp.
    Nếu videos còn <= refill_threshold và next_token tồn tại -> spawn worker fetch page_token.
    page_token có thể được truyền khi gọi để tiếp tục từ token đó.
    """
    lock = get_lock(key)
    
    with lock:
        if key not in cache:
            if OFFLINE_MODE:
                raise HTTPException(status_code=503, detail="Server offline, cache chưa có dữ liệu")
            videos, token = fetch_func(page_token=None)
            cache[key] = (videos, token)

        videos, next_token = cache[key]
        result = []

        while videos and len(result) < batch_size:
            result.append(videos.popleft())

        # Nếu còn <= threshold → spawn refill thread
        if len(videos) <= refill_threshold and page_token and not OFFLINE_MODE:
            # Submit refill task
            executor.submit(_refill_cache_worker, cache, key, fetch_func, lock, page_token=next_token)

    # Chọn video phù hợp
    if key.startswith("search_"):
        selected = prioritize_videos_by_keyword(result, key[len("search_"):])
    else:
        filtered = filter_internal_duplicates(result)
        selected = weighted_random(filtered, k=len(filtered))

    return selected, next_token

def _refill_cache_worker(cache, key, fetch_func, lock, page_token):
    """Worker chạy trong background để refill cache"""
    try:
        new_videos, new_token = fetch_func(page_token=page_token)
        with lock:
            if key in cache:
                videos, _ = cache[key]
                videos.extend(new_videos)
                cache[key] = (videos, new_token)
    except Exception as e:
        print(f"[REFILL ERROR] Key={key} | Error: {e}")

def normalize_text(text):
    """Loại bỏ dấu, khoảng trắng, lowercase"""
    text = unicodedata.normalize('NFD', text)
    text = ''.join(c for c in text if unicodedata.category(c) != 'Mn')
    text = text.replace(" ", "")
    return text.lower()

def generate_substrings(keyword, min_len=3, max_len=6):
    """Sinh tất cả substring dài từ min_len đến max_len"""
    keyword_len = len(keyword)
    substrings = set()
    for l in range(min_len, max_len + 1):
        for i in range(keyword_len - l + 1):
            substrings.add(keyword[i:i+l])
    return substrings

def prioritize_videos_by_keyword(videos, keyword, min_len=3, max_len=6):
    """
    Phiên bản hoàn chỉnh ưu tiên video theo keyword:
    - tách từ dựa vào khoảng trắng (URL-decoded)
    - match ký tự thường + số
    - tính khoảng cách, thứ tự xuất hiện các từ
    - giảm điểm video đã xuất hiện gần đây
    """
    # Giải mã URL (nếu keyword URL-encoded)
    decoded_keyword = unquote(keyword)
    words = decoded_keyword.split()  # tách thành từ
    keyword_norm = normalize_text(decoded_keyword)
    
    # Tách số và chữ
    keyword_numbers = re.findall(r"\d+", keyword_norm)
    keyword_chars = re.sub(r"\d+", "", keyword_norm)
    
    substrings = generate_substrings(keyword_chars, min_len, max_len)
    
    candidates = []

    for v in videos:
        score = 0
        title_norm = normalize_text(v.get("title", ""))
        desc_norm = normalize_text(v.get("description", "") if "description" in v else "")

        # --- 1. Matching ký tự thường ---
        for sub in substrings:
            if sub in title_norm:
                score += len(sub)
            elif sub in desc_norm:
                score += len(sub) // 2

        # --- 2. Matching chuỗi số ---
        title_numbers = re.findall(r"\d+", title_norm)
        desc_numbers = re.findall(r"\d+", desc_norm)
        for num in keyword_numbers:
            if num in title_numbers:
                score += len(num) * 10
            elif num in desc_numbers:
                score += len(num) * 5

        # --- 3. Matching từ theo thứ tự & khoảng cách ---
        positions = []
        for word in words:
            idx = title_norm.find(normalize_text(word))
            if idx != -1:
                positions.append(idx)
        if positions == sorted(positions) and len(positions) == len(words):
            # Các từ xuất hiện đúng thứ tự → tăng điểm
            score += 10
            # Tăng thêm nếu các từ gần nhau (khoảng cách nhỏ)
            for i in range(len(positions) - 1):
                dist = positions[i+1] - positions[i]
                if dist <= 10:  # khoảng cách ký tự nhỏ
                    score += max(0, 5 - dist//2)

        # --- 4. Giảm điểm nếu video đã xuất hiện gần đây ---
        if v["id"] in recent_videos:
            score *= 0.1

        candidates.append((score, v))

    # Chỉ giữ video có score > 0
    candidates = [c for c in candidates if c[0] > 0]
    if not candidates:
        return []

    # --- Chọn ngẫu nhiên theo trọng số ---
    weights, vids = zip(*candidates)
    selected = random.choices(vids, weights=weights, k=min(len(vids), 5))

    # Cập nhật recent_videos
    for v in selected:
        recent_videos.append(v["id"])
    if len(recent_videos) > 50:
        recent_videos[:] = recent_videos[-50:]

    return selected

def filter_internal_duplicates(videos):
    """Lọc video trùng dựa trên recent_videos nội bộ"""
    seen = set(recent_videos)
    unique_list = []
    for v in videos:
        if v["id"] not in seen:
            seen.add(v["id"])
            unique_list.append(v)
    return unique_list

def weighted_random(videos, k=5):
    """Chọn video ngẫu nhiên, giảm trọng số video gần đây"""
    if not videos:
        return []

    candidates = [(0.1 if v["id"] in recent_videos else 1.0, v) for v in videos]
    weights, vids = zip(*candidates)
    selected = random.choices(vids, weights=weights, k=min(k, len(vids)))

    for v in selected:
        recent_videos.append(v["id"])
    if len(recent_videos) > 50:
        recent_videos[:] = recent_videos[-50:]

    return selected

def mix_trending_with_keywords(videos, keywords, ratio=0.5):
    """
    Trộn trending + keyword videos theo tỷ lệ (mặc định 50-50).
    - Ưu tiên key đại diện -> key thường -> trending gốc.
    """
    key_videos = []
    normal_videos = []
    trending_videos = []

    for v in videos:
        score = 0
        title = v.get("title", "").lower()
        desc = v.get("description", "").lower()

        matched = False
        for k, meta in keywords.items():
            k_norm = k.lower()
            if k_norm in title or k_norm in desc:
                matched = True
                if meta.get("is_representative"):
                    score += 500
                else:
                    score += 200 * meta.get("count", 1)
        if matched:
            if score >= 500:
                key_videos.append((score, v))
            else:
                normal_videos.append((score, v))
        else:
            trending_videos.append((0, v))

    # Sắp xếp từng nhóm
    key_videos.sort(key=lambda x: -x[0])
    normal_videos.sort(key=lambda x: -x[0])

    # Ghép nhóm key lại
    all_key_videos = [v for _, v in (key_videos + normal_videos)]
    trending_videos = [v for _, v in trending_videos]

    # Tính số lượng lấy theo ratio
    total = len(videos)
    n_key = int(total * ratio)
    n_trend = total - n_key

    result = all_key_videos[:n_key] + trending_videos[:n_trend]
    return result

def fetch_with_fallback(url, params):
    """Gọi API YouTube thông qua Key Manager"""
    if OFFLINE_MODE:
        raise HTTPException(status_code=503, detail={
            "message": "Server offline, chỉ trả dữ liệu cache",
            "error": True,
            "can_retry": True
        })

    # Xin key từ Key Manager
    key_info = acquire()
    if "api_key" not in key_info:
        raise Exception("Không có key khả dụng từ Key Manager")
    api_key = key_info["api_key"]

    try:
        params["key"] = api_key
        r = requests.get(url, params=params, timeout=10)
        data = r.json()
        if r.status_code == 200 and "error" not in data:
            # Trả key lại cho pool
            release(api_key)
            return data
        else:
            # Quota exceeded → báo lại Key Manager
            if r.status_code == 403 or "quota" in str(data).lower():
                report_quota_exceeded(api_key)
            else:
                release(api_key)
            raise Exception(data.get("error", {}).get("message", "Unknown error"))
    except Exception as e:
        # Có lỗi → cũng báo lại
        report_quota_exceeded(api_key)
        raise

# ==================== FETCH FUNCTIONS ====================
def fetch_youtube_search(q=None, page_token=None, max_results=50):
    url = "https://www.googleapis.com/youtube/v3/search"
    params = {"part": "snippet", "type": "video", "maxResults": max_results}
    if q: params["q"] = q
    if page_token: params["pageToken"] = page_token

    data = fetch_with_fallback(url, params)

    video_ids = [item["id"]["videoId"] for item in data.get("items", [])]
    videos_info = {}

    if video_ids:
        details_url = "https://www.googleapis.com/youtube/v3/videos"
        details_params = {
            "part": "contentDetails",
            "id": ",".join(video_ids)
        }
        details_data = fetch_with_fallback(details_url, details_params)
        for item in details_data.get("items", []):
            videos_info[item["id"]] = item["contentDetails"].get("duration", "")

    videos = deque()
    for item in data.get("items", []):
        vid = item["id"].get("videoId")
        if not vid:
            continue
        snippet = item["snippet"]
        videos.append({
            "id": vid,
            "title": snippet["title"],
            "channel": snippet["channelTitle"],
            "duration": videos_info.get(vid, ""),
            "publishedAt": snippet.get("publishedAt", ""),
            "thumbnail": snippet["thumbnails"]["medium"]["url"],
            "link": f"https://www.youtube.com/watch?v={vid}",
            "isShorts": "shorts" in snippet["title"].lower()
        })

    return videos, data.get("nextPageToken")

def fetch_youtube_trending(page_token=None, total_results=50,
                           key_ratio=0.7, max_keys=5):
    """
    Lấy video trending + video search theo top key.
    - key_ratio: % video từ key (vd 0.7 = 70%)
    - max_keys: số lượng key hot tối đa dùng để bổ sung
    """
    # --- trending gốc ---
    url = "https://www.googleapis.com/youtube/v3/videos"
    params = {
        "part": "snippet,contentDetails",
        "chart": "mostPopular",
        "regionCode": "VN",
        "maxResults": total_results
    }
    if page_token:
        params["pageToken"] = page_token

    data = fetch_with_fallback(url, params)
    trending_raw = [{
        "id": item["id"],
        "title": item["snippet"]["title"],
        "channel": item["snippet"]["channelTitle"],
        "duration": item["contentDetails"].get("duration", ""),
        "publishedAt": item["snippet"].get("publishedAt", ""),
        "thumbnail": item["snippet"]["thumbnails"]["medium"]["url"],
        "link": f"https://www.youtube.com/watch?v={item['id']}",
        "isShorts": False 
    } for item in data.get("items", [])]

    # --- xác định số lượng key vs trending ---
    n_key = math.floor(total_results * key_ratio)
    n_trend = total_results - n_key

    # --- chọn top key theo count ---
    sorted_keys = sorted(search_keywords.items(),
                         key=lambda kv: -kv[1].get("count", 0))
    top_keys = sorted_keys[:max_keys]

    # tổng count của top key để chia slot
    total_count = sum(meta.get("count", 1) for _, meta in top_keys) or 1

    key_videos = []

    with ThreadPoolExecutor(max_workers=5) as executor:  # tùy chỉnh max_workers
        futures = {}
        for key, meta in top_keys:
            count = meta.get("count", 1)
            share = int(n_key * count / total_count)
            if share <= 0:
                continue
            futures[executor.submit(fetch_youtube_search, q=key, max_results=share)] = key

        for future in as_completed(futures):
            key = futures[future]
            try:
                extra_videos, _ = future.result()
                key_videos.extend(extra_videos)
            except Exception as e:
                print(f"Lỗi fetch keyword {key}: {e}")

    # --- trộn key + trending ---
    combined = key_videos[:n_key] + trending_raw[:n_trend]

    # --- loại trùng ID ---
    seen = set()
    unique_videos = []
    for v in combined:
        if v["id"] not in seen:
            seen.add(v["id"])
            unique_videos.append(v)

    return deque(unique_videos), data.get("nextPageToken")

# ==================== CACHE HANDLER ====================
def get_cached_videos(cache, key, fetch_func, batch_size=5, refill_threshold=10):
    """Trả về batch_size video từ cache, refill khi còn lại <= refill_threshold"""
    if key not in cache:
        if OFFLINE_MODE:
            raise HTTPException(status_code=503, detail={
                "message": "Server offline, dữ liệu chưa có trong cache",
                "error": True,
                "can_retry": True
            })
        videos, token = fetch_func(page_token=None)
        cache[key] = (videos, token)

    videos, next_token = cache[key]
    result = []

    while videos and len(result) < batch_size:
        result.append(videos.popleft())

    if len(videos) <= refill_threshold and next_token:
        refill_cache_background(cache, key, fetch_func, page_token=next_token)

    if key.startswith("search_"):
        selected = prioritize_videos_by_keyword(result, key[len("search_"):])
    else:
        filtered = filter_internal_duplicates(result)
        selected = weighted_random(filtered, k=len(filtered))

    return selected, next_token
def get_cached_trending(cache, key, fetch_func, batch_size=5, refill_threshold=10):
    """Lấy trending + bổ sung video liên quan top search keywords"""
    if key not in cache:
        videos, token = fetch_func(page_token=None)
        cache[key] = (videos, token)

    videos, next_token = cache[key]
    result = []

    while videos and len(result) < batch_size:
        result.append(videos.popleft())

    # Refill khi còn lại nhỏ hơn threshold
    if len(videos) <= refill_threshold and next_token and not OFFLINE_MODE:
        refill_cache_background(cache, key, fetch_func, page_token=next_token)

    # --- Bổ sung video từ top search keywords ---
    if not OFFLINE_MODE:
        for keyword, _ in top_search_keywords(n=3):
            try:
                search_videos_batch, _ = fetch_youtube_search(q=keyword, max_results=3)
                result.extend(search_videos_batch)
            except:
                continue

    # Lọc trùng
    filtered = filter_internal_duplicates(result)
    selected = weighted_random(filtered, k=len(filtered))

    selected = mix_trending_with_keywords(selected, search_keywords, ratio=0.8)

    return selected, next_token

# ==================== ROUTES ====================
@app.get("/watched")
def watched_video(url: str, title: Optional[str] = None):
    """
    Ghi nhận video mà người dùng đã xem.
    - url: link video
    - title: tiêu đề video (nếu client gửi sẵn, không cần fetch)
    """
    check_network()
    if OFFLINE_MODE:
        return {"status": "offline", "message": "Không có mạng, dữ liệu không được lưu"}

    # Nếu không có title, cố gắng fetch từ YouTube API
    video_title = title
    if not video_title:
        video_id = None
        try:
            parsed = urlparse(url)
            if parsed.hostname in ["www.youtube.com", "youtube.com"]:
                qs = parse_qs(parsed.query)
                video_id = qs.get("v", [None])[0]
            elif parsed.hostname == "youtu.be":
                video_id = parsed.path.lstrip("/")
            
            if video_id:
                data = fetch_with_fallback(
                    "https://www.googleapis.com/youtube/v3/videos",
                    {"part": "snippet", "id": video_id}
                )
                items = data.get("items", [])
                if items:
                    video_title = items[0]["snippet"]["title"]
        except:
            pass

    if video_title:
        words = video_title.split()
        for w in words:
            if len(w) > 2:
                key = normalize_text(w)
                if key in search_keywords:
                    # đã có → update như search thường
                    track_search_keyword(w)
                else:
                    # mới → thêm như key đại diện
                    search_keywords[key] = {
                        "count": 1,
                        "mention_count": 1,
                        "last_seen": time.time(),
                        "expire_seconds": DEFAULT_EXPIRE_SECONDS,
                        "is_representative": True   # đánh dấu là key đại diện
                    }
        prune_keywords()
        save_search_keywords()

        return {"status": "ok", "message": f"Video '{video_title}' đã được ghi nhận"}
    else:
        return {"status": "fail", "message": "Không lấy được tiêu đề video"}
    
@app.get("/search")
def search_videos(q: Optional[str] = None):
    check_network()
    if not q:
        return trending()
    track_search_keyword(q)
    q_clean = q.replace(" ", "")
    key = f"search_{q_clean}"
    videos, token = get_cached_videos(
        search_cache,
        key,
        lambda page_token=None: fetch_youtube_search(q=q, page_token=page_token)
    )

    shorts = [v for v in videos if v.get("isShorts", False)]
    normal_videos = [v for v in videos if not v.get("isShorts", False)]

    return {
        "videos": normal_videos,
        "shorts": shorts,
        "nextPageToken": token
    }

@app.get("/trending")
def trending():
    check_network()
    load_search_keywords()
    key = "trending"
    videos, token = get_cached_trending(
        trending_cache,
        key,
        lambda page_token=None: fetch_youtube_trending(page_token=page_token)
    )

    shorts = [v for v in videos if v.get("isShorts", False)]
    normal_videos = [v for v in videos if not v.get("isShorts", False)]

    return {
        "videos": normal_videos,
        "shorts": shorts,
        "nextPageToken": token
    }

@app.get("/thumbnail")
def get_thumbnail(url: str):
    try:
        r = requests.get(url)
        return Response(content=r.content, media_type="image/jpeg")
    except:
        return Response(status_code=404)
    
@app.get("/all-keys")
def all_keys():
    prune_keywords()
    # Trả tất cả key, kể cả key đại diện
    return {"keys": [(k, v) for k,v in search_keywords.items()]}

@app.get("/keywords")
def get_keywords(top: int = Query(20)):
    prune_keywords()
    # Chỉ lấy key search thực
    top_keys = sorted(
        [(k, v) for k,v in search_keywords.items() if not v.get("is_representative", False)],
        key=lambda x: -x[1]["count"]
    )[:top]
    return {"top_keywords": [(k, v["count"]) for k,v in top_keys]}

@app.post("/keywords/reset")
def reset_keywords():
    search_keywords.clear()
    save_search_keywords()
    return {"status": "ok", "message": "Đã reset tất cả từ khóa"}

@app.post("/keywords/delete")
def delete_keyword(keyword: str = Query(...)):
    key = normalize_text(keyword)
    if key in search_keywords:
        del search_keywords[key]
        save_search_keywords()
        return {"status": "ok", "message": f"Đã xóa từ khóa '{keyword}'"}
    else:
        return {"status": "fail", "message": f"Từ khóa '{keyword}' không tồn tại"}

@app.post("/batch-videos")
def receive_batch_videos(videos: List[VideoItem]):
    """Nhận batch video từ client"""
    for v in videos:
        # Xử lý, lưu vào cache, log, hoặc gửi tiếp cho hệ thống xử lý khác
        print(f"Received video: {v.id} - {v.title}")
    return {"status": "ok", "received": len(videos)}
    

# ==================== RUN ====================
if __name__ == "__main__":
    import uvicorn
    load_search_keywords()
    uvicorn.run(app, host="127.0.0.1", port=8000)
