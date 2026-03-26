import logging
from logging.handlers import RotatingFileHandler
import sys
import os

# ==========================
# Cấu hình chung
# ==========================
ENABLE_CONSOLE = True
ENABLE_FILE = True
LOG_FILE_PATH = "log.log"

DEFAULT_LOG_LEVEL = "DEBUG"
CONSOLE_LOG_LEVEL = "DEBUG"
FILE_LOG_LEVEL = "DEBUG"

LOG_COLORS = {
    "DEBUG": "\033[94m",
    "INFO": "\033[92m",
    "WARNING": "\033[93m",
    "ERROR": "\033[91m",
    "CRITICAL": "\033[41m",
}
RESET_COLOR = "\033[0m"

LEVELS = {
    "CRITICAL": logging.CRITICAL,
    "ERROR": logging.ERROR,
    "WARNING": logging.WARNING,
    "INFO": logging.INFO,
    "DEBUG": logging.DEBUG,
    "NOTSET": logging.NOTSET
}

logger_level = LEVELS.get(DEFAULT_LOG_LEVEL.upper(), logging.DEBUG)
console_level = LEVELS.get(CONSOLE_LOG_LEVEL.upper(), logging.INFO)
file_level = LEVELS.get(FILE_LOG_LEVEL.upper(), logging.DEBUG)

# ==========================
# Formatter màu cho console
# ==========================
class ColoredFormatter(logging.Formatter):
    def format(self, record):
        color = LOG_COLORS.get(record.levelname, "")
        msg = super().format(record)
        if color:
            msg = f"{color}{msg}{RESET_COLOR}"
        return msg

console_formatter = ColoredFormatter(
    "%(asctime)s | %(levelname)s | %(name)s | %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)

file_formatter = logging.Formatter(
    "%(asctime)s | %(levelname)s | %(name)s | %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)

# ==========================
# Factory tạo logger cho từng module
# ==========================
def get_logger(name=None):
    """
    Trả về logger đặt tên theo module.
    Nếu name=None -> dùng __name__ của module import.
    """
    logger_name = name or __name__
    logger = logging.getLogger(logger_name)
    logger.setLevel(logger_level)

    # Chỉ thêm handler nếu logger chưa có (tránh trùng lặp)
    if not logger.hasHandlers():
        if ENABLE_CONSOLE:
            console_handler = logging.StreamHandler(sys.stdout)
            console_handler.setLevel(console_level)
            console_handler.setFormatter(console_formatter)
            logger.addHandler(console_handler)

        if ENABLE_FILE:
            os.makedirs(os.path.dirname(LOG_FILE_PATH) or ".", exist_ok=True)
            file_handler = RotatingFileHandler(
                LOG_FILE_PATH, maxBytes=5*1024*1024, backupCount=3, encoding="utf-8"
            )
            file_handler.setLevel(file_level)
            file_handler.setFormatter(file_formatter)
            logger.addHandler(file_handler)
    
    return logger
