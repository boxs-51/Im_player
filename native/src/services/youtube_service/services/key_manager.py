import requests
from config.settings import KEY_SERVER, LOCAL_KEYS, local_index

def acquire():
    global local_index
    try:
        r = requests.post(f"{KEY_SERVER}/acquire", timeout=3)
        r.raise_for_status()
        return r.json()
    except:
        if not LOCAL_KEYS:
            raise Exception("No local API keys configured!")
        key = LOCAL_KEYS[local_index]
        local_index = (local_index + 1) % len(LOCAL_KEYS)
        return {"api_key": key, "fallback": True}

def release(k):
    try:
        requests.post(f"{KEY_SERVER}/release", json={"api_key": k}, timeout=3)
    except:
        # fallback: chỉ log chứ không crash
        print(f"[release] Key server unavailable, released key locally: {k}")

def report_quota_exceeded(k):
    try:
        requests.post(f"{KEY_SERVER}/report", json={"api_key": k, "error": "quota exceeded"}, timeout=3)
    except:
        # fallback: chỉ log chứ không crash
        print(f"[report] Key server unavailable, quota exceeded for key: {k}")
