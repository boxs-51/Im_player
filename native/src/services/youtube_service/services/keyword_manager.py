import json
import time
from config.settings import (
    KEYS_FILE,
    DEFAULT_EXPIRE_SECONDS,
    MAX_INTERACTIONS
)
from utils.text_utils import normalize_text

# =============================
#  STORAGE
# =============================
search_keywords = {}      # Dict chứa mọi keyword và metadata


# =============================
#  FILE LOAD / SAVE
# =============================
def load_search_keywords():
    try:
        with open(KEYS_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
            search_keywords.clear()
            search_keywords.update(data)
    except FileNotFoundError:
        search_keywords.clear()


def save_search_keywords():
    """Lưu file search_keywords.json"""
    with open(KEYS_FILE, "w", encoding="utf-8") as f:
        json.dump(search_keywords, f, ensure_ascii=False, indent=2)


# =============================
#  KEYWORD CLEANING
# =============================
def prune_keywords():
    """Xoá keyword hết hạn expire_seconds"""
    now = time.time()
    to_delete = []

    for k, meta in search_keywords.items():
        expire = meta.get("expire_seconds", DEFAULT_EXPIRE_SECONDS)
        last_seen = meta.get("last_seen", 0)

        if expire is not None and (now - last_seen) > expire:
            to_delete.append(k)

    for k in to_delete:
        del search_keywords[k]

    if to_delete:
        save_search_keywords()


# =============================
#  TRACKING / UPDATE KEYWORDS
# =============================
def track_search_keyword(keyword: str):
    """
    Thêm hoặc cập nhật keyword
    - tăng count
    - tăng mention_count
    - cập nhật last_seen
    - nếu vượt MAX_INTERACTIONS thì bỏ expire_seconds
    """
    key = normalize_text(keyword)
    now = time.time()

    if key in search_keywords:
        meta = search_keywords[key]
        meta["count"] = meta.get("count", 0) + 1
        meta["mention_count"] = meta.get("mention_count", 0) + 1
        meta["last_seen"] = now

        # lưu lại cách viết thực tế của người dùng
        variants = meta.setdefault("variants", [])
        if keyword not in variants:
            variants.append(keyword)

        # Nếu được nhắc nhiều → trở thành key vĩnh viễn
        if meta["mention_count"] >= MAX_INTERACTIONS:
            meta["expire_seconds"] = None  # sống vĩnh viễn
    else:
        # Tạo keyword mới
        search_keywords[key] = {
            "count": 1,
            "mention_count": 1,
            "last_seen": now,
            "expire_seconds": DEFAULT_EXPIRE_SECONDS,
            "is_representative": False,

            "original": keyword,
            "variants": [keyword],
        }

    # Cleanup
    prune_keywords()
    save_search_keywords()


# =============================
#  GET TOP KEYS
# =============================
def top_search_keywords(n=5):
    """Trả về top N từ khóa theo count"""
    prune_keywords()   # làm sạch trước
    sorted_keys = sorted(
        [(k, v.get("count", 0)) for k, v in search_keywords.items()],
        key=lambda x: -x[1]
    )
    return sorted_keys[:n]
