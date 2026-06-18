from ranking.plugins.base import RankingPlugin

class UserHistoryPlugin(RankingPlugin):
    name = "user_history"

    def __init__(self):
        self.seen = {}  # user_id -> set(video_id)

    def score(self, item, context):
        uid = context.get("user_id")
        if not uid:
            return 0.0

        if item.id in self.seen.get(uid, set()):
            return -1.0
        return 0.5

    def on_served(self, item, context):
        uid = context.get("user_id")
        if not uid:
            return
        self.seen.setdefault(uid, set()).add(item.id)
