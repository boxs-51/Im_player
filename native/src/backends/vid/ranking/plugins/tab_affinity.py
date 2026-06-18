from ranking.plugins.base import RankingPlugin

class TabAffinityPlugin(RankingPlugin):
    name = "tab_affinity"

    def __init__(self):
        self.affinity = {}  # user_id -> tab -> score

    def score(self, item, context):
        uid = context.get("user_id")
        tab = context.get("tab")
        return self.affinity.get(uid, {}).get(tab, 0.0)

    def on_served(self, item, context):
        uid = context.get("user_id")
        tab = context.get("tab")
        if not uid:
            return
        self.affinity.setdefault(uid, {})
        self.affinity[uid][tab] = self.affinity[uid].get(tab, 0) + 0.1
