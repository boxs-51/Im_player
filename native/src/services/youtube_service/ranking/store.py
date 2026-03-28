import time

class RankedItem:
    def __init__(self, video):
        self.video = video
        self.id = video.get("id")
        self.first_seen = time.time()
        self.last_served = None
        self.serve_count = 0
        self.priority = 1.0


class TabStore:
    def __init__(self):
        self.items = {}  # video_id -> RankedItem
