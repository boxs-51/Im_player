from collections import deque
from cachetools import TTLCache
from concurrent.futures import ThreadPoolExecutor
import threading
import aiohttp
import asyncio
from pathlib import Path
import hashlib
from config.settings import OFFLINE_MODE , MEMORY_CACHE_SIZE, MEMORY_CACHE_TTL, FILE_CACHE_DIR ,MAX_CONCURRENT_FETCH
from utils.video_utils import weighted_random, filter_internal_duplicates, prioritize_videos_by_keyword
from config.logger_config import get_logger

log = get_logger("vaideos_cache")

# ================================
executor = ThreadPoolExecutor(max_workers=5)

# ========= Config =========
fetch_semaphore = asyncio.Semaphore(MAX_CONCURRENT_FETCH)

url_locks = {}  
cache_locks = {}

memory_cache = TTLCache(maxsize=MEMORY_CACHE_SIZE, ttl=MEMORY_CACHE_TTL)
search_cache = TTLCache(maxsize=MEMORY_CACHE_SIZE, ttl=MEMORY_CACHE_TTL)
trending_cache = TTLCache(maxsize=MEMORY_CACHE_SIZE, ttl=MEMORY_CACHE_TTL)

GLOBAL_STORE = {
    "videos": deque(),
    "index": {}
}

def push_to_global_store(videos):
    added = 0
    for v in videos:
        vid = v.get("id")
        if not vid:
            continue
        if vid not in GLOBAL_STORE["index"]:
            GLOBAL_STORE["videos"].append(v)
            GLOBAL_STORE["index"][vid] = v
            added += 1

    log.debug(f"[GLOBAL STORE] added={added} total={len(GLOBAL_STORE['videos'])}")


# ================================
#  INIT CACHE
# ================================
def get_initial_cache(cache, key, fetch_func):
    log.debug(f"[INIT CACHE] key={key}")
    videos, token = fetch_func(page_token=None)
    log.debug(f"[INIT CACHE] key={key} token={token} items={len(videos)}")
    push_to_global_store(videos)
    cache[key] = (deque(videos), token)


# ================================
#  LOCK HELPERS
# ================================
def get_lock(key):
    if key not in cache_locks:
        cache_locks[key] = threading.Lock()
        log.debug(f"[LOCK] created key={key}")
    return cache_locks[key]


# ================================
#  REFILL WORKER (BACKGROUND)
# ================================
def _refill_worker(cache, key, fetch_func, lock, page_token):
    log.debug(f"[REFILL WORKER] key={key} fetch page_token={page_token}")
    try:
        videos, new_token = fetch_func(page_token=page_token)
        push_to_global_store(videos)

        log.debug(f"[REFILL WORKER] key={key} fetched={len(videos)} new_token={new_token}")

        with lock:
            if key in cache:
                old_videos, _old_token = cache[key]

                existing_ids = {v.get("id") for v in old_videos}
                new_videos = [v for v in videos if v.get("id") not in existing_ids]

                before = len(old_videos)
                old_videos.extend(new_videos)
                after = len(old_videos)

                # 🔥 cập nhật token mới
                cache[key] = (old_videos, new_token)

                log.debug(
                    f"[REFILL WORKER] key={key} cache_size {before}→{after} token={new_token}"
                )

    except Exception as e:
        log.error(f"[REFILL ERROR] key={key} error={e}")


# ================================
#  REFILL TRIGGER
# ================================
def refill_cache(cache, key, fetch_func, page_token, threshold=10):
    lock = get_lock(key)

    with lock:
        if key not in cache:
            log.warning(f"[REFILL] key={key} not in cache")
            return

        videos, next_token = cache[key]
        size = len(videos)

        log.debug(f"[REFILL CHECK] key={key} size={size} threshold={threshold} token={next_token}")

        if size <= threshold and next_token and not OFFLINE_MODE:
            log.debug(f"[REFILL START] key={key} using token={next_token}")
            executor.submit(
                _refill_worker,
                cache,
                key,
                fetch_func,
                lock,
                next_token
            )
        else:
            log.debug(f"[REFILL SKIP] key={key}")
# ================================
#  MAIN CACHE GETTER (SEARCH)
# ================================
def get_cached_videos(cache, key, fetch_func, batch_size=5, refill_threshold=10):

    log.debug(f"[GET CACHED] key={key} batch={batch_size}")

    if key not in cache:
        log.debug(f"[GET CACHED] key={key} cache miss → init")
        if OFFLINE_MODE:
            raise Exception("Server offline – cache chưa có dữ liệu.")
        get_initial_cache(cache, key, fetch_func)

    lock = get_lock(key)

    with lock:
        videos, next_token = cache[key]
        before = len(videos)

        batch = []
        seen_ids = set()
        while videos and len(batch) < batch_size:
            v = videos.popleft()
            vid = v.get("id")
            if vid and vid not in seen_ids:
                batch.append(v)
                seen_ids.add(vid)

        after = len(videos)
        cache[key] = (videos, next_token)

        log.debug(
            f"[GET CACHED] key={key} pop {len(batch)} items cache {before}→{after} token={next_token}"
        )

    if next_token:
        refill_cache(cache, key, fetch_func, next_token, refill_threshold)

    with lock:
        _vtmp, next_token = cache[key]

    # keyword mode
    if key.startswith("search_"):
        keyword = key[len("search_"):]
        log.debug(f"[SEARCH MODE] key={key} keyword='{keyword}'")

        # Lấy từ global store, nhưng loại trùng với batch vừa pop
        global_results = [
            v for v in get_from_global_by_keyword(keyword, limit=50)
            if v.get("id") not in seen_ids
        ]

        log.debug(f"[SEARCH MODE] global_hits={len(global_results)}")

        if global_results:
            combined = batch + global_results
            filtered = filter_internal_duplicates(combined)
            final_videos = prioritize_videos_by_keyword(filtered, keyword)
            return weighted_random(final_videos, k=len(final_videos)), next_token

    filtered = filter_internal_duplicates(batch)
    log.debug(f"[RETURN] key={key} filtered={len(filtered)} final={len(filtered)}")
    return weighted_random(filtered, k=len(filtered)), next_token


# ================================
#  TRENDING GETTER
# ================================
def get_cached_trending(cache, key, fetch_func, batch_size=5, refill_threshold=10):
    log.debug(f"[GET TRENDING] key={key} batch={batch_size}")
    if key not in cache:
        log.debug(f"[GET TRENDING] key={key} cache miss → init")
        if OFFLINE_MODE:
            raise Exception("Server offline – cache chưa có dữ liệu.")
        # ❌ Thay vì fetch trực tiếp, dùng chung hàm init
        get_initial_cache(cache, key, fetch_func)

    lock = get_lock(key)

    with lock:
        videos, next_token = cache[key]
        batch = []
        seen_ids = set()
        while videos and len(batch) < batch_size:
            v = videos.popleft()
            vid = v.get("id")
            if vid and vid not in seen_ids:
                batch.append(v)
                seen_ids.add(vid)
        cache[key] = (videos, next_token)
        log.debug(
            f"[GET TRENDING] key={key} pop {len(batch)} items cache_size={len(videos)} token={next_token}"
        )

    if next_token:
        refill_cache(cache, key, fetch_func, next_token, refill_threshold)

    filtered = filter_internal_duplicates(batch)
    return weighted_random(filtered, k=len(filtered)), next_token

# ================================
#  GLOBAL STORE SEARCH
# ================================
def get_from_global_by_keyword(keyword, limit=10):
    results = []
    for v in GLOBAL_STORE["videos"]:
        title = v.get("title", "").lower()
        if keyword.lower() in title:
            results.append(v)
        if len(results) >= limit:
            break
    return results


# ================================
#  THUMBNAIL FETCHING
# ================================
def get_url_lock(url: str) -> asyncio.Lock:
    if url not in url_locks:
        url_locks[url] = asyncio.Lock()
    return url_locks[url]


def url_to_filename(url: str) -> Path:
    h = hashlib.md5(url.encode()).hexdigest()
    return FILE_CACHE_DIR / f"{h}.jpg"


async def fetch_thumbnail(url: str) -> bytes:
    file_path = url_to_filename(url)

    if file_path.exists():
        return file_path.read_bytes()

    async with fetch_semaphore:
        async with aiohttp.ClientSession() as session:
            async with session.get(url) as resp:
                if resp.status != 200:
                    raise Exception(f"Failed to fetch {url}")
                content = await resp.read()

    file_path.write_bytes(content)
    return content
