def resolve_tabs(video):
    title = video.get("title", "").lower()
    tabs = []

    if any(k in title for k in ["music", "mv", "song"]):
        tabs.append("music")

    if any(k in title for k in ["game", "gaming", "play"]):
        tabs.append("gaming")

    if any(k in title for k in ["news", "breaking"]):
        tabs.append("news")

    return tabs or ["others"]
