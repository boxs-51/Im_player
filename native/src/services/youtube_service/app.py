from fastapi import FastAPI, Query, Response
from fastapi.middleware.cors import CORSMiddleware
import requests
import time

from config.logger_config import get_logger
from utils.network import check_network
from models.video_item import VideoItem
from models.manager_key import KeywordDelete
from services.ranking_engine import RankingEngine, resolve_tabs
from services.caches import GLOBAL_STORE

from services.keyword_manager import (
    search_keywords, track_search_keyword, prune_keywords,
    load_search_keywords, save_search_keywords, top_search_keywords
)

from services.caches import (
    search_cache, trending_cache,memory_cache,
    get_cached_videos, get_cached_trending , get_url_lock,fetch_thumbnail
)

from services.youtube_fetch import fetch_youtube_search, fetch_youtube_trending
from utils.text_utils import normalize_text

from ranking.engine import RankingEngine
from ranking.resolver  import resolve_tabs
from ranking.plugins.freshness import FreshnessPlugin
from ranking.plugins.user_history import UserHistoryPlugin
from ranking.plugins.tab_affinity import TabAffinityPlugin

app = FastAPI()
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_headers=["*"],
    allow_methods=["*"]
)
ranking_engine = RankingEngine(
    plugins=[
        FreshnessPlugin(),
        UserHistoryPlugin(),
        TabAffinityPlugin()
    ]
)

@app.get("/watched")
def watched_video(url: str, title: str | None = None):
    check_network()

    if not title:
        vid = None
        try:
            from urllib.parse import urlparse, parse_qs
            p = urlparse(url)
            if "youtube" in p.hostname:
                vid = parse_qs(p.query).get("v", [None])[0]
            elif "youtu.be" in p.hostname:
                vid = p.path.lstrip("/")

            if vid:
                data = fetch_youtube_search(q=vid)[0]
        except:
            pass

    if title:
        words = title.split()
        for w in words:
            if len(w) > 2:
                track_search_keyword(w)
        prune_keywords()
        save_search_keywords()

    return {"status": "ok"}


@app.get("/search")
def search(q: str | None = None):
    check_network()

    if not q:
        return trending()

    track_search_keyword(q)
    key = "search_" + q.replace(" ", "")

    videos, token = get_cached_videos(
        search_cache,
        key,
        lambda page_token=None: fetch_youtube_search(q=q, page_token=page_token)
    )

    shorts = [v for v in videos if v.get("isShorts")]
    normal = [v for v in videos if not v.get("isShorts")]

    return {"videos": normal, "shorts": shorts, "nextPageToken": token}


@app.get("/trending")
def trending(tab: str | None = None, user_id: str | None = None):
    check_network()
    load_search_keywords()

    if tab:
        if ranking_engine.need_refill(tab):
            videos, token = get_cached_trending(
                trending_cache,
                "trending",
                lambda page_token=None: fetch_youtube_trending(
                    tab=tab,
                    page_token=page_token
                )
            )
            ranking_engine.add_videos(videos, resolve_tabs)

        videos = ranking_engine.select(tab, user_id=user_id)
        return {"tab": tab, "videos": videos}
    try:
        videos, token = get_cached_trending(
            trending_cache,
            "trending",
            lambda page_token=None: fetch_youtube_trending(page_token=page_token)
        )
    except Exception as e:
        # QUOTA HẾT → DEGRADED MODE
        print("[TRENDING FALLBACK]", e)
            # ưu tiên cache trước
        if "trending" in trending_cache:
            videos, token = trending_cache["trending"]
            videos = list(videos)[:10]
            token = None
        else:
            # fallback global store
            videos = list(GLOBAL_STORE["videos"])[:10]
            token = None

    ranking_engine.add_videos(videos, resolve_tabs)
    return {
        "videos": videos,
        "active_tabs": list(ranking_engine.tabs.keys()),
        "nextPageToken": token
    }



@app.get("/thumbnail")
async def thumbnail(url: str):
    url_lock = get_url_lock(url)

    # Lock per URL để tránh spam fetch cùng URL
    async with url_lock:
        # 1️⃣ check memory cache
        if url in memory_cache:
            return Response(content=memory_cache[url], media_type="image/jpeg")

        # 2️⃣ fetch (file cache hoặc mạng)
        try:
            content = await fetch_thumbnail(url)
        except Exception:
            return Response(status_code=404)

        # 3️⃣ lưu vào memory cache
        memory_cache[url] = content

        return Response(content=content, media_type="image/jpeg")


@app.get("/all-keys")
def all_keys():
    prune_keywords()
    return {"keys": list(search_keywords.items())}


@app.get("/keywords")
def keywords(top: int = Query(20)):
    prune_keywords()
    top_items = sorted(
        [(k, v) for k, v in search_keywords.items() if not v.get("is_representative")],
        key=lambda x: -x[1]["count"]
    )[:top]
    return {
        "top_keywords": [
            (v.get("original", k), v["count"])
            for k, v in top_items
        ]
    }


@app.post("/keywords/reset")
def reset_keywords():
    search_keywords.clear()
    save_search_keywords()
    return {"status": "ok"}


@app.post("/keywords/delete")
def delete_keyword(data: KeywordDelete):
    k = normalize_text(data.keyword)
    if k in search_keywords:
        del search_keywords[k]
        save_search_keywords()
        return {"status": "ok"}
    return {"status": "fail", "message": "Không tồn tại"}


@app.post("/batch-videos")
def batch(videos: list[VideoItem]):
    for v in videos:
        print(f"Received: {v.id} - {v.title}")
    return {"received": len(videos)}
@app.get("/ping")
def ping():
    """
    Kiểm tra backend có alive không.
    Trả về thời gian server xử lý request (ms) và status.
    """
    start = time.time()
    return {
        "status": "ok",
        "server_time_ms": int((time.time() - start) * 1000),
        "timestamp": int(time.time())
    }

if __name__ == "__main__":
    import uvicorn
    load_search_keywords()
    uvicorn.run(app, host="127.0.0.1", port=8000)
