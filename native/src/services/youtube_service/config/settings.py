import os
import time

# --- Imports ---
from pathlib import Path
# --- Cấu hình cache ---

MEMORY_CACHE_SIZE = 500
MEMORY_CACHE_TTL = 3600  # 1h
MAX_CONCURRENT_FETCH = 10  # limit toàn server
FILE_CACHE_DIR = Path("./thumbnail_cache")
FILE_CACHE_DIR.mkdir(exist_ok=True)

# tạo thư mục nếu chưa có
KEY_SERVER = "http://127.0.0.1:6000"

LOCAL_KEYS = ["AIzaSyDmlB96pEhcAV_m6Koy_H9aTODzn4TBU9A"]
#AIzaSyCu0Y-0ht_-2CWGHE7QGlAEfaQbIGq2wD8
#AIzaSyDmlB96pEhcAV_m6Koy_H9aTODzn4TBU9A
local_index = 0

OFFLINE_MODE = False
MAX_INTERACTIONS = 500
DEFAULT_EXPIRE_SECONDS = 7 * 24 * 3600

def find_root_dir(marker_filename="PROJECT_ROOT_1.dat", start_dir=None, max_levels=10):
    """
    Tìm thư mục gốc dựa trên presence của file marker
    """
    if start_dir is None:
        start_dir = os.path.abspath(os.path.dirname(__file__))
    
    current = start_dir
    for _ in range(max_levels):
        if marker_filename in os.listdir(current):
            return current
        parent = os.path.abspath(os.path.join(current, ".."))
        if parent == current:
            break  # đã lên tới root của ổ đĩa
        current = parent
    raise FileNotFoundError(f"Không tìm thấy {marker_filename} trong {start_dir} và các thư mục cha")

# --- Khởi tạo ROOT_DIR ---
ROOT_DIR = find_root_dir()
DATA_DIR = os.path.join(ROOT_DIR, "data")
KEYS_FILE = os.path.join(DATA_DIR, "search_keywords.json")

# tạo thư mục data nếu chưa có
os.makedirs(DATA_DIR, exist_ok=True)



