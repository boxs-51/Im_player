from abc import ABC, abstractmethod

class RankingPlugin(ABC):
    name = "base"

    @abstractmethod
    def score(self, item, context):
        """
        Trả về score (float)
        item  : RankedItem
        context: dict (user, tab, now, ...)
        """
        pass

    def on_served(self, item, context):
        """Hook khi video được trả về"""
        pass
