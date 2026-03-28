import requests
from config.settings import OFFLINE_MODE

def check_network():
    global OFFLINE_MODE
    try:
        requests.get("https://www.google.com", timeout=3)
        OFFLINE_MODE = False
        return True
    except:
        OFFLINE_MODE = True
        return False
