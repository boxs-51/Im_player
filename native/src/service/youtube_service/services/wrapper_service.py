import asyncio
from typing import List, Dict, Optional
from services.caches import (
    get_cached_search,
    get_cached_trending,
    push_to_global_store,
    prioritize_videos_by_keyword,
    fetch_thumbnail
)
from config.settings import OFFLINE_MODE

# =================== Config ===================
DEFAULT_BATCH_SIZE = 5
KEYWORD_RATIO = 0.7  # % video từ keyword search
MAX_KEYWORDS = 5     # số từ khóa chính lấy top
PRELOAD_THUMBNAILS = True

# ---------------- Global keyword store (demo) ----------------
# Thực tế nên lấy từ DB hoặc Key Manager
SEARCH_KEYWORDS = {
    "python": {"count": 120},
    "asyncio": {"count": 95},
    "youtube api": {"count": 80},
    "tutorial": {"count": 60},
    "shorts": {"count": 50}
}

# =================== Wrapper Service ===================
async def get_videos(batch_size=DEFAULT_BATCH_SIZE,
                     include_trending=True,
                     include_keywords=True,
                     key_ratio=KEYWORD_RATIO) -> List[Dict]:
    """
    Fetch videos combined: trending + keyword search
    """
    tasks = []
    trending_videos: List[Dict] = []
    keyword_videos: List[Dict] = []

    # ---------------- 1️⃣ Trending ----------------
    if include_trending:
        tasks.append(get_cached_trending(batch_size=batch_size))

    # ---------------- 2️⃣ Keyword search ----------------
    if include_keywords and SEARCH_KEYWORDS:
        # lấy top N từ khóa
        sorted_keys = sorted(SEARCH_KEYWORDS.items(), key=lambda kv: -kv[1]["count"])[:MAX_KEYWORDS]
        keywords = [k for k, _ in sorted_keys]

        # số lượng video từ keywords
        n_key_videos = int(batch_size * key_ratio)
        if n_key_videos > 0:
            for kw in keywords:
                tasks.append(get_cached_search(f"search_{kw}", kw, batch_size=n_key_videos))

    # ---------------- Run tasks async ----------------
    results = await asyncio.gather(*tasks, return_exceptions=True)
    
    # Phân loại
    idx = 0
    if include_trending:
        trending_videos = results[idx] if isinstance(results[idx], list) else []
        idx += 1
    if include_keywords and SEARCH_KEYWORDS:
        for i, kw in enumerate(keywords):
            kw_result = results[idx + i]
            if isinstance(kw_result, list):
                keyword_videos.extend(kw_result)

    # ---------------- Deduplicate + prioritize ----------------
    seen_ids = set()
    final_videos: List[Dict] = []

    # Trending ưu tiên
    for v in trending_videos:
        vid = v.get("id")
        if vid and vid not in seen_ids:
            seen_ids.add(vid)
            final_videos.append(v)

    # Keyword video ưu tiên global + keyword match
    for v in keyword_videos:
        vid = v.get("id")
        if vid and vid not in seen_ids:
            seen_ids.add(vid)
            # ưu tiên nếu keyword xuất hiện trong title
            kw_matched = any(kw.lower() in v["title"].lower() for kw in keywords)
            v["_keyword_match"] = kw_matched
            final_videos.append(v)

    # Sắp xếp keyword video match lên đầu
    final_videos.sort(key=lambda v: v.get("_keyword_match", False), reverse=True)

    # Cắt batch cuối
    final_videos = final_videos[:batch_size]

    # ---------------- Preload thumbnails ----------------
    if PRELOAD_THUMBNAILS:
        await asyncio.gather(*(fetch_thumbnail(v["thumbnail"]) for v in final_videos if "thumbnail" in v))

    return final_videos
