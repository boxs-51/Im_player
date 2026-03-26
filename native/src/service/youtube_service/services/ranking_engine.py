import time

class RankedItem:
    def __init__(self, video):
        self.video = video
        self.first_seen = time.time()
        self.last_served = None
        self.serve_count = 0
        self.priority = 1.0
class TabStore:
    def __init__(self):
        self.items = {}  # video_id -> RankedItem
        self.last_decay = time.time()
class RankingEngine:
    def __init__(
        self,
        decay_factor=0.6,
        recovery_factor=0.2,
        reset_after=3600
    ):
        self.tabs = {}
        self.decay_factor = decay_factor
        self.recovery_factor = recovery_factor
        self.reset_after = reset_after
    def add_videos(self, videos, tab_resolver):
        for v in videos:
            vid = v.get("id")
            if not vid:
                continue

            tabs = tab_resolver(v)
            for tab in tabs:
                store = self.tabs.setdefault(tab, TabStore())

                if vid not in store.items:
                    store.items[vid] = RankedItem(v)
    def _apply_decay(self, store):
        now = time.time()

        if now - store.last_decay < self.reset_after:
            return

        for item in store.items.values():
            if item.last_served and now - item.last_served > self.reset_after:
                item.priority = min(
                    item.priority + self.recovery_factor,
                    1.0
                )

        store.last_decay = now
    def select(self, tab, limit=5):
        store = self.tabs.get(tab)
        if not store:
            return []

        self._apply_decay(store)

        items = list(store.items.values())
        items.sort(
            key=lambda x: (x.priority, x.first_seen),
            reverse=True
        )

        selected = items[:limit]

        now = time.time()
        for item in selected:
            item.priority *= self.decay_factor
            item.serve_count += 1
            item.last_served = now

        return [item.video for item in selected]
    def need_refill(self, tab, min_priority=0.4, min_count=5):
        store = self.tabs.get(tab)
        if not store:
            return True

        good = [
            i for i in store.items.values()
            if i.priority >= min_priority
        ]
        return len(good) < min_count
ranking_engine = RankingEngine()   
def resolve_tabs(video):
    title = video.get("title", "").lower()

    tabs = []
    if "music" in title or "mv" in title:
        tabs.append("music")
    if "game" in title:
        tabs.append("gaming")

    return tabs or ["others"]
