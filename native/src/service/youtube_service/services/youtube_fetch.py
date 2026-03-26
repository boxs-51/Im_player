from collections import deque
import math
from services.key_manager import acquire, release, report_quota_exceeded
from services.keyword_manager import search_keywords
from utils.text_utils import normalize_text
from config.logger_config import get_logger
from config.settings import OFFLINE_MODE
import time
import logging

log = get_logger("youtube_fetch")

YOUTUBE_QUOTA_EXCEEDED = False
QUOTA_RESET_AT = None
QUOTA_COOLDOWN = 60 * 60  # 1 giờ

# =============================
# Fetch with quota handling
# =============================
import requests

def fetch_with_fallback(url, params):
    if OFFLINE_MODE:
        log.warning("OFFLINE_MODE is enabled, skipping fetch.")
        raise Exception("OFFLINE_MODE enabled")

    global YOUTUBE_QUOTA_EXCEEDED, QUOTA_RESET_AT

    if YOUTUBE_QUOTA_EXCEEDED:
        if QUOTA_RESET_AT and time.time() < QUOTA_RESET_AT:
            log.warning(f"YouTube quota exhausted (cooldown), reset at {time.ctime(QUOTA_RESET_AT)}")
            raise Exception("YouTube quota exhausted (cooldown)")
        else:
            YOUTUBE_QUOTA_EXCEEDED = False
            QUOTA_RESET_AT = None
            log.info("Quota cooldown expired, resuming requests.")

    key_info = acquire()
    if "api_key" not in key_info:
        log.error("Không nhận được key từ Key Server.")
        raise Exception("Không nhận được key từ Key Server.")

    api_key = key_info["api_key"]

    try:
        params["key"] = api_key
        log.debug(f"Fetching URL: {url} with params: {params}")
        r = requests.get(url, params=params, timeout=10)
        data = r.json()

        if r.status_code == 200 and "error" not in data:
            release(api_key)
            return data

        error_msg = str(data.get("error", {}).get("message", "")).lower()
        if r.status_code == 403 and "quota" in error_msg:
            report_quota_exceeded(api_key)
            YOUTUBE_QUOTA_EXCEEDED = True
            QUOTA_RESET_AT = time.time() + QUOTA_COOLDOWN
            log.warning(f"YouTube quota exceeded for key {api_key}. Cooldown until {time.ctime(QUOTA_RESET_AT)}")
        else:
            release(api_key)

        raise Exception(error_msg or "YouTube API error")

    except requests.RequestException as e:
        release(api_key)
        log.error(f"Network error while calling YouTube: {e}")
        raise Exception("Network error while calling YouTube")

    except ValueError as e:
        release(api_key)
        log.error(f"Invalid JSON from YouTube: {e}")
        raise Exception("Invalid JSON from YouTube")


# =============================
# Batch fetch video details
# =============================
def fetch_video_details_bulk(video_ids, parts=None):
    """
    Lấy chi tiết tất cả video ID theo batch 50 ID/request
    """
    if parts is None:
        parts = "snippet,contentDetails,statistics,status,player,liveStreamingDetails"

    results = {}
    batch_size = 50
    for i in range(0, len(video_ids), batch_size):
        batch = video_ids[i:i+batch_size]
        url = "https://www.googleapis.com/youtube/v3/videos"
        params = {
            "part": parts,
            "id": ",".join(batch)
        }
        data = fetch_with_fallback(url, params)
        for item in data.get("items", []):
            vid = item["id"]
            results[vid] = item
        log.debug(f"Fetched batch of {len(batch)} videos, total fetched: {len(results)}")
    return results


# =============================
# Search videos
# =============================
def fetch_youtube_search(q=None, page_token=None, max_results=50):
    url = "https://www.googleapis.com/youtube/v3/search"
    params = {
        "part": "snippet",
        "type": "video",
        "maxResults": max_results
    }
    if q:
        params["q"] = q
    if page_token:
        params["pageToken"] = page_token

    data = fetch_with_fallback(url, params)

    # Lấy tất cả video ID, lọc trùng
    video_ids = [
        it["id"]["videoId"]
        for it in data.get("items", [])
        if "videoId" in it["id"]
    ]
    video_ids = list(set(video_ids))
    log.debug(f"Found {len(video_ids)} unique video IDs for query '{q}'")

    # Fetch details 1 lần theo batch
    details = fetch_video_details_bulk(video_ids)

    # Build kết quả
    videos = deque()
    for vid in video_ids:
        item = details[vid]
        snippet = item["snippet"]
        stats = item.get("statistics", {})
        content = item.get("contentDetails", {})

        videos.append({
            "id": vid,
            "title": snippet.get("title", ""),
            "channel": snippet.get("channelTitle", ""),
            "duration": content.get("duration", ""),
            "views": stats.get("viewCount", "0"),
            "likes": stats.get("likeCount", "0"),
            "publishedAt": snippet.get("publishedAt", ""),
            "thumbnail": snippet.get("thumbnails", {}).get("medium", {}).get("url", ""),
            "link": f"https://www.youtube.com/watch?v={vid}",
            "isLive": snippet.get("liveBroadcastContent", "none"),
            "isShorts": "shorts" in snippet.get("title", "").lower(),
        })
    log.info(f"Returning {len(videos)} videos for query '{q}'")
    return videos, data.get("nextPageToken")


# =============================
# Trending videos
# =============================
def fetch_youtube_trending(
    page_token=None,
    total_results=50,
    key_ratio=1,
    max_keys=5,
    include_trending=True,
    include_keywords=True,
    region="VN"
):
    all_ids = []
    next_token_trending = None
    next_token_search = None

    # =================
    # 1) Lấy ID trending
    # =================
    if include_trending:
        url = "https://www.googleapis.com/youtube/v3/videos"
        params = {
            "part": "id",
            "chart": "mostPopular",
            "regionCode": region,
            "maxResults": total_results
        }
        if page_token:
            params["pageToken"] = page_token

        data = fetch_with_fallback(url, params)
        trending_ids = [item["id"] for item in data.get("items", []) if "id" in item]
        next_token_trending = data.get("nextPageToken")
        log.info(f"Trending IDs fetched: {len(trending_ids)}")
        all_ids.extend(trending_ids)
    else:
        trending_ids = []

    # =================
    # 2) Lấy ID keyword search
    # =================
    if include_keywords and search_keywords:
        n_key = math.floor(total_results * key_ratio)
        sorted_keys = sorted(search_keywords.items(), key=lambda kv: -kv[1].get("count", 0))[:max_keys]
        q = " | ".join(k for k, _ in sorted_keys)
        log.info(f"Fetching video IDs for keywords: {q}")

        url_search = "https://www.googleapis.com/youtube/v3/search"
        params_search = {
            "part": "id",
            "type": "video",
            "q": q,
            "maxResults": n_key
        }
        if page_token:
            params_search["pageToken"] = page_token

        data_search = fetch_with_fallback(url_search, params_search)
        keyword_ids = [item["id"]["videoId"] for item in data_search.get("items", []) if "videoId" in item["id"]]
        next_token_search = data_search.get("nextPageToken")
        log.info(f"Keyword IDs fetched: {len(keyword_ids)}")
        all_ids.extend(keyword_ids)
    else:
        keyword_ids = []

    # =================
    # 3) Dedupe
    # =================
    all_ids = list(set(all_ids))
    if not all_ids:
        log.info("No videos found.")
        return deque(), None
    log.info(f"Total unique video IDs to fetch details: {len(all_ids)}")

    # =================
    # 4) Fetch details batch 50
    # =================
    all_details = fetch_video_details_bulk(all_ids)

    # =================
    # 5) Build deque kết quả
    # =================
    videos = deque()
    for vid in all_ids:
        item = all_details[vid]
        snippet = item["snippet"]
        stats = item.get("statistics", {})
        content = item.get("contentDetails", {})

        videos.append({
            "id": vid,
            "title": snippet.get("title", ""),
            "channel": snippet.get("channelTitle", ""),
            "duration": content.get("duration", ""),
            "views": stats.get("viewCount", "0"),
            "likes": stats.get("likeCount", "0"),
            "publishedAt": snippet.get("publishedAt", ""),
            "thumbnail": snippet.get("thumbnails", {}).get("medium", {}).get("url", ""),
            "link": f"https://www.youtube.com/watch?v={vid}",
            "isLive": snippet.get("liveBroadcastContent", "none"),
            "isShorts": "shorts" in snippet.get("title", "").lower(),
        })

    # =================
    # 6) Chọn nextPageToken
    # =================
    if include_trending and not include_keywords:
        next_token = next_token_trending
    elif include_keywords and not include_trending:
        next_token = next_token_search
    else:
        next_token = next_token_search or next_token_trending

    log.info(f"Returning {len(videos)} trending+keyword videos.")
    return videos, next_token
