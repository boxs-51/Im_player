import random
import re
from .text_utils import normalize_text, generate_substrings
from urllib.parse import unquote

recent_videos = []

def filter_internal_duplicates(videos):
    seen = set(recent_videos)
    unique = []
    for v in videos:
        if v["id"] not in seen:
            seen.add(v["id"])
            unique.append(v)
    return unique


def weighted_random(videos, k=5):
    if not videos:
        return []
    weights, vids = zip(*[
        (0.1 if v["id"] in recent_videos else 1.0, v)
        for v in videos
    ])
    selected = random.choices(vids, weights=weights, k=min(k, len(vids)))
    for v in selected:
        recent_videos.append(v["id"])
    if len(recent_videos) > 50:
        del recent_videos[:-50]
    return selected


def prioritize_videos_by_keyword(videos, keyword, min_len=3, max_len=6):
    decoded = unquote(keyword)
    words = decoded.split()
    norm = normalize_text(decoded)

    numbers = re.findall(r"\d+", norm)
    chars = re.sub(r"\d+", "", norm)
    substrs = generate_substrings(chars, min_len, max_len)

    candidates = []
    for v in videos:
        score = 0
        tn = normalize_text(v.get("title", ""))
        dn = normalize_text(v.get("description", ""))

        for sub in substrs:
            if sub in tn: score += len(sub)
            elif sub in dn: score += len(sub) // 2

        tn_numbers = re.findall(r"\d+", tn)
        for n in numbers:
            if n in tn_numbers: score += len(n) * 10

        pos = []
        for w in words:
            idx = tn.find(normalize_text(w))
            if idx != -1: pos.append(idx)

        if pos == sorted(pos) and len(pos) == len(words):
            score += 10
            for i in range(len(pos) - 1):
                d = pos[i+1] - pos[i]
                if d <= 10: score += max(0, 5 - d // 2)

        if v["id"] in recent_videos:
            score *= 0.1

        candidates.append((score, v))

    candidates = [c for c in candidates if c[0] > 0]
    if not candidates: return []

    weights, vids = zip(*candidates)
    selected = random.choices(vids, weights=weights, k=min(5, len(vids)))

    for v in selected:
        recent_videos.append(v["id"])
    if len(recent_videos) > 50:
        del recent_videos[:-50]

    return selected
