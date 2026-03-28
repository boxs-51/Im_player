from flask import Flask, request, jsonify
from queue import Queue
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta
import threading
import time

app = Flask(__name__)

# ================== CONFIG ==================
API_KEYS = [
    #"AIzaSyAmKbo7kXEDy_hus3qWYHQuK_xsJs1x9r0",
    #"AIzaSyCGcRx6CXvsQxnNsBa26Wmcbd4azzecby0",
    #"AIzaSyBm9045OacVo1GeAVFBYbYjSFPGr6nPkH0",
    #"AIzaSyDHpBJw2l9DDiQzw-qUx87UuZgj5n24LTo",
    #"AIzaSyC7_ZQR8RkMcCjS60GO_u_kFcR4djzhZM0",
    #"AIzaSyAqTo63PBpIWOaf8P8eg-KX-dUld6jba-Y",
    #"AIzaSyA3NWZmt-VeJ7_-xuJz0jprPrlABoGT1gw",
    #"AIzaSyBvjmR1bBVd7h0EmcJ8XmhmGlcRP0do_tY",
    #"AIzaSyCu0Y-0ht_-2CWGHE7QGlAEfaQbIGq2wD8",
    #"AIzaSyDx_CMVPhB_yncBSE1mCfUtdw34zHW6QGM",

    #"AIzaSyCu0Y-0ht_-2CWGHE7QGlAEfaQbIGq2wD8",

    #"AIzaSyB68nlsKowgypKWSTfTeZffyhDE-QlU3_Q",
    #"AIzaSyAa0lCHcVpl6-_2UQNVtW2JF7F4JTahBg8",
    #"AIzaSyDmlB96pEhcAV_m6Koy_H9aTODzn4TBU9A",


]


MAX_USAGE = 1000
key_queue = Queue()
key_info = {}  # dict lưu thông tin chi tiết của từng key

# ThreadPoolExecutor cho xử lý đồng thời
executor = ThreadPoolExecutor(max_workers=20)

# Khởi tạo pool
for k in API_KEYS:
    key_info[k] = {"usage": 0, "dead": False, "in_use": False}
    key_queue.put(k)

# Auto reset usage daily
def reset_usage_daily():
    while True:
        now = datetime.now()
        next_reset = datetime.combine(now.date() + timedelta(days=1), datetime.min.time())
        sleep_seconds = (next_reset - now).total_seconds()
        time.sleep(sleep_seconds)
        for k, info in key_info.items():
            info["usage"] = 0
            if info["dead"]:
                info["dead"] = False
        print("[RESET] All key usage reset")

threading.Thread(target=reset_usage_daily, daemon=True).start()
# ============================================

def acquire_key_logic():
    """Lấy key từ queue"""
    while not key_queue.empty():
        key = key_queue.get()
        info = key_info[key]
        if info["dead"] or info["usage"] >= MAX_USAGE:
            continue  # bỏ qua key này
        info["in_use"] = True
        info["usage"] += 1
        return key
    return None

@app.route("/acquire", methods=["POST"])
def acquire_key():
    key = executor.submit(acquire_key_logic).result()
    if key:
        print(f"[ACQUIRE] Key {key} assigned (usage={key_info[key]['usage']})")
        return jsonify({"api_key": key})
    return jsonify({"error": "No free key available"}), 503

@app.route("/release", methods=["POST"])
def release_key():
    data = request.get_json()
    if not data or "api_key" not in data:
        return jsonify({"error": "Missing api_key"}), 400
    key = data["api_key"]
    if key not in key_info:
        return jsonify({"error": "Invalid key"}), 400

    info = key_info[key]
    if not info["in_use"]:
        return jsonify({"error": "Key not in use"}), 400

    info["in_use"] = False
    if not info["dead"] and info["usage"] < MAX_USAGE:
        key_queue.put(key)  # trả lại queue
    print(f"[RELEASE] Key {key} released")
    return jsonify({"status": "released"})

@app.route("/report", methods=["POST"])
def report_key():
    data = request.get_json()
    if not data or "api_key" not in data or "error" not in data:
        return jsonify({"error": "Missing api_key or error"}), 400

    key = data["api_key"]
    err = data["error"]
    if key not in key_info:
        return jsonify({"error": "Invalid key"}), 400

    info = key_info[key]
    info["in_use"] = False
    if "quota" in err.lower() or "403" in err:
        info["dead"] = True
        print(f"[REPORT] Key {key} disabled due to error: {err}")
    else:
        # Nếu lỗi bình thường, trả lại queue
        if not info["dead"] and info["usage"] < MAX_USAGE:
            key_queue.put(key)
    return jsonify({"status": "reported"})

@app.route("/status", methods=["GET"])
def status():
    total_keys = len(key_info)
    free_keys = sum(1 for k, v in key_info.items() if not v["in_use"] and not v["dead"] and v["usage"] < MAX_USAGE)
    dead_keys = sum(1 for v in key_info.values() if v["dead"])
    return jsonify({
        "total_keys": total_keys,
        "free_keys": free_keys,
        "dead_keys": dead_keys,
        "keys": key_info
    })

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=6000, debug=True, use_reloader=False)
