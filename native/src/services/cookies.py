import json
import traceback
import browser_cookie3
import os
import glob
import time
import logging
import pythoncom
pythoncom.CoInitialize()


def check_cookie_valid(cookies):
    required_tokens = ["SID", "HSID", "SSID", "SAPISID", "APISID"]
    content = "".join([c.value for c in cookies])
    return all(token in content for token in required_tokens)


def detect_edge_cookies():
    import pythoncom
    pythoncom.CoInitialize()
    import browser_cookie3
    return browser_cookie3.edge(domain_name='.youtube.com')

def safe_edge_cookies():
    import pythoncom
    pythoncom.CoInitialize()
    import browser_cookie3
    return browser_cookie3.edge(domain_name='.youtube.com')

def save_cookies_to_file(cookie_file_path, cookies):
    seen = set()
    unique_cookies = []
    for cookie in cookies:
        key = (cookie.domain, cookie.path, cookie.name)
        if key not in seen:
            seen.add(key)
            unique_cookies.append(cookie)

    with open(cookie_file_path, 'w', encoding='utf-8') as f:
        f.write("# Netscape HTTP Cookie File\n")
        for cookie in unique_cookies:
            f.write(
                f"{cookie.domain}\t"
                f"{'TRUE' if cookie.domain.startswith('.') else 'FALSE'}\t"
                f"{cookie.path}\t"
                f"{'TRUE' if cookie.secure else 'FALSE'}\t"
                f"{cookie.expires if cookie.expires else 0}\t"
                f"{cookie.name}\t"
                f"{cookie.value}\n"
            )
    print(f"Đã lưu cookie YouTube ra file: {cookie_file_path}")
    return cookie_file_path

def save_youtube_cookies_to_file(cookie_file_path, browser_name='both'):
    print(f"Bắt đầu lấy cookie YouTube từ {browser_name}...")
    try:
        combined_cookies = []

        if browser_name in ('chrome', 'both'):
            try:
                cj_chrome = browser_cookie3.chrome(domain_name='.youtube.com')
                combined_cookies.extend(list(cj_chrome))
                print("Lấy cookie Chrome thành công")
            except Exception as e:
                print("Không lấy được cookie Chrome:", e)

        if browser_name in ('edge', 'both'):
            try:
                cj_edge = browser_cookie3.edge(domain_name='.youtube.com')
                combined_cookies.extend(list(cj_edge))
                print("Lấy cookie Edge thành công")
            except Exception as e:
                print("Không lấy được cookie Edge:", e)

        if not combined_cookies:
            print("Không tìm thấy cookie nào từ trình duyệt được chọn")
            return None

        return save_cookies_to_file(cookie_file_path, combined_cookies)

    except Exception:
        print("Lỗi lấy cookie tổng quát:")
        traceback.print_exc()
        return None

def load_cookies_from_json_file(json_path):
    print(f"Đang đọc cookie từ file JSON: {json_path}")
    if not os.path.isfile(json_path):
        print("File JSON cookie không tồn tại.")
        return None

    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        json_cookies = data.get('cookies', [])
        if not json_cookies:
            print("File JSON không có mục 'cookies' hoặc rỗng.")
            return None

        # Lấy thời gian hết hạn lớn nhất
        max_expiration = 0
        for c in json_cookies:
            exp = c.get('expirationDate', 0)
            if exp and exp > max_expiration:
                max_expiration = exp

        now = time.time()
        if max_expiration < now:
            print(f"File JSON cookie đã hết hạn (max expiration: {max_expiration}, hiện tại: {now}), bỏ qua.")
            return None

        class SimpleCookie:
            def __init__(self, domain, path, name, value, secure, expires):
                self.domain = domain
                self.path = path
                self.name = name
                self.value = value
                self.secure = secure
                self.expires = expires

        cookies = []
        for c in json_cookies:
            cookies.append(SimpleCookie(
                domain=c.get('domain', ''),
                path=c.get('path', '/'),
                name=c.get('name', ''),
                value=c.get('value', ''),
                secure=c.get('secure', False),
                expires=int(c.get('expirationDate', 0))
            ))

        print(f"Đã đọc được {len(cookies)} cookie từ JSON.")
        return cookies
    except Exception:
        print("Lỗi khi đọc file JSON cookie:")
        traceback.print_exc()
        return None

def find_latest_json_cookie_file(folder_path, pattern='www.youtube.com_*.json'):
    files = glob.glob(os.path.join(folder_path, pattern))
    if not files:
        print(f"Không tìm thấy file JSON cookie nào theo mẫu '{pattern}' trong thư mục {folder_path}")
        return None
    latest_file = max(files, key=os.path.getmtime)
    print(f"File JSON cookie mới nhất được tìm thấy: {latest_file}")
    return latest_file

def detect_and_save_cookies(cookie_file_path, json_folder=r'D:\MPVPLAYER\imgui_player\temp'):
    print("Bắt đầu phát hiện trình duyệt có cookie YouTube...")

    # Thử lấy cookie Chrome trước
    try:
        cj_chrome = browser_cookie3.chrome(domain_name='.youtube.com')
        cookies_chrome = list(cj_chrome)
        if cookies_chrome:
            print("Phát hiện cookie Chrome, lưu lại...")
            return save_youtube_cookies_to_file(cookie_file_path, browser_name='chrome')
        else:
            print("Không tìm thấy cookie YouTube trong Chrome")
    except Exception as e:
        print("Lỗi khi lấy cookie Chrome:", e)

    # Nếu Chrome không được thì thử lấy cookie Edge
    try:
        cj_edge =  safe_edge_cookies()
        cookies_edge = list(cj_edge)
        if cookies_edge:
            print("Phát hiện cookie Edge, lưu lại...")
            return save_youtube_cookies_to_file(cookie_file_path, browser_name='edge')
        else:
            print("Không tìm thấy cookie YouTube trong Edge")
    except Exception as e:
        print("Lỗi khi lấy cookie Edge:", e)

    # Nếu không lấy được cookie trình duyệt thì thử load file JSON cookie mới nhất
    print("Thử lấy cookie từ file JSON mới nhất trong thư mục...")
    latest_json_file = find_latest_json_cookie_file(json_folder)
    if latest_json_file:
        cookies_json = load_cookies_from_json_file(latest_json_file)
        if cookies_json:
            print("Lưu cookie lấy từ file JSON...")
            return save_cookies_to_file(cookie_file_path, cookies_json)
        else:
            print("Không thể lấy cookie từ file JSON.")
    else:
        print("Không tìm thấy file JSON cookie nào để lấy.")

    print("Không tìm thấy cookie YouTube trên cả Chrome, Edge và JSON")
    return None


if __name__ == "__main__":
    path = r'D:\MPVPLAYER\imgui_player\temp\cookies.txt'
    detect_and_save_cookies(path)
