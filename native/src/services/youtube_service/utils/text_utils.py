import unicodedata
import re
from urllib.parse import unquote

def normalize_text(text):
    text = unicodedata.normalize('NFD', text)
    text = ''.join(c for c in text if unicodedata.category(c) != 'Mn')
    return text.replace(" ", "").lower()

def generate_substrings(keyword, min_len=3, max_len=6):
    subs = set()
    for L in range(min_len, max_len + 1):
        for i in range(len(keyword) - L + 1):
            subs.add(keyword[i:i+L])
    return subs
