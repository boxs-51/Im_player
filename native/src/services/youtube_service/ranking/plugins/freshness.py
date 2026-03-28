from ranking.plugins.base import RankingPlugin
import time

class FreshnessPlugin(RankingPlugin):
    name = "freshness"

    def score(self, item, context):
        score = item.priority
        if item.last_served:
            age = time.time() - item.last_served
            if age > 1800:
                score += 0.3
        return score

    def on_served(self, item, context):
        item.priority *= 0.6
        item.last_served = time.time()
        item.serve_count += 1
