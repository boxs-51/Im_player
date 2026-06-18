from ranking.store import RankedItem, TabStore

class RankingEngine:
    def __init__(self, plugins=None):
        self.plugins = plugins or []
        self.tabs = {}  # tab -> TabStore

    # =========================
    # ADD DATA
    # =========================
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

    # =========================
    # SELECT
    # =========================
    def select(self, tab, user_id=None, limit=5):
        store = self.tabs.get(tab)
        if not store:
            return []

        context = {
            "user_id": user_id,
            "tab": tab
        }

        scored = []
        for item in store.items.values():
            score = 0.0
            for plugin in self.plugins:
                score += plugin.score(item, context)
            scored.append((score, item))

        scored.sort(key=lambda x: x[0], reverse=True)
        selected = [item for _, item in scored[:limit]]

        for item in selected:
            for plugin in self.plugins:
                plugin.on_served(item, context)

        return [item.video for item in selected]

    # =========================
    # REFILL CHECK
    # =========================
    def need_refill(self, tab, min_items=5):
        store = self.tabs.get(tab)
        if not store:
            return True
        return len(store.items) < min_items

