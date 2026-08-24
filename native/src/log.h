#pragma once
#include "globals.h"

#include <windows.h>

#include <iostream>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <type_traits>

// =================== Hệ thống phân loại Log ===================

enum class LogLevel {
    Info,
    Warning,
    Error,
    Debug,
    Critical
};

enum class LogCategory : uint32_t {
    None      = 0,
    System    = 1 << 0,  // Các sự kiện hệ thống chung, core
    AI        = 1 << 1,  // Hoạt động của trợ lý AI
    Audio     = 1 << 2,  // Các sự kiện từ AudioFilterManager, EBU-R128
    Render    = 1 << 3,  // Luồng render, shader, FBO
    UI        = 1 << 4,  // Tương tác người dùng, popup
    Network   = 1 << 5,  // yt-dlp, streaming, cache
    Config    = 1 << 6,  // Lưu/tải file cấu hình
    Sync      = 1 << 7,  // Đồng bộ filter, pipeline
    Safety    = 1 << 8,  // Hệ thống an toàn
    Bypass    = 1 << 9,  // Các sự kiện bypass
    All       = 0xFFFFFFFF // Mask để lấy tất cả
};

inline LogCategory operator|(LogCategory a, LogCategory b) {
    return static_cast<LogCategory>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

// =================== Cơ chế Bảo vệ & Định dạng Chuỗi An toàn (C++17) ===================

namespace Detail {

    template <typename T>
    decltype(auto) ConvertArg(T&& arg) {
        using Decayed = std::decay_t<T>;
        
        if constexpr (std::is_same_v<Decayed, std::string>) {
            return arg.c_str();
        } 
        else if constexpr (std::is_same_v<Decayed, std::string_view>) {
            return arg.data();
        }
        else if constexpr (std::is_pointer_v<Decayed> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<Decayed>>, char>) {
            const char* p = static_cast<const char*>(arg);
            return p ? p : "(null)";
        }
        else {
            return std::forward<T>(arg);
        }
    }

    inline int SafeSnprintf(char* buf, size_t count, const char* fmt, ...) {
        int result = -1;
        va_list args;
        va_start(args, fmt);

        __try {
            result = vsnprintf(buf, count, fmt, args);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            result = -2;
        }

        va_end(args);
        return result;
    }
}

template <typename... Args>
std::string FormatString(const char* fmt, Args&&... args) {
    if (!fmt) return "[LOG ERROR: null format string]";

    if constexpr (sizeof...(Args) == 0) {
        return std::string(fmt);
    } else {
        int size_s = Detail::SafeSnprintf(nullptr, 0, fmt, Detail::ConvertArg(std::forward<Args>(args))...);
        
        if (size_s == -2) {
            char errBuf[512];
            sprintf_s(errBuf, "[LOG FMT ERROR] Bad format specifier or type mismatch in fmt: \"%s\"", fmt);
            OutputDebugStringA(errBuf);
            return std::string(errBuf);
        }

        if (size_s <= 0) return std::string();

        size_t size = static_cast<size_t>(size_s);
        std::string buf(size, '\0');
        
        int write_res = Detail::SafeSnprintf(&buf[0], size + 1, fmt, Detail::ConvertArg(std::forward<Args>(args))...);
        if (write_res == -2) {
            return "[LOG FMT ERROR: Access Violation during formatting]";
        }

        return buf;
    }
}

// =================== Hệ thống quản lý Log History ===================

struct LogMessage {
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point last_updated; // Thời gian gọi gần nhất
    std::string message;
    std::string key;
    std::string location;
    LogLevel level;
    LogCategory category;
    uint32_t repeat_count = 1; // Số lần log bị trùng nội dung
};

class LogHistoryManager {
public:
    static LogHistoryManager& GetInstance() {
        static LogHistoryManager instance;
        return instance;
    }

    void AddOrUpdateLog(
        const std::string& key,
        const std::string& message,
        const std::string& location,
        LogLevel level = LogLevel::Info,
        LogCategory category = LogCategory::System)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto now = std::chrono::system_clock::now();

        // Kiểm tra log cuối cùng có cùng key và nội dung không
        if (!m_logs.empty() && m_logs.back().key == key && m_logs.back().message == message) {
            m_logs.back().repeat_count++;
            m_logs.back().last_updated = now;
            return;
        }

        m_logs.push_back({
            now,
            now, // last_updated khởi tạo bằng timestamp
            message,
            key,
            location,
            level,
            category,
            1    // repeat_count mặc định
        });

        if (m_logs.size() > MAX_LOGS) {
            m_logs.erase(m_logs.begin());
        }
    }

    std::vector<LogMessage> GetRecentLogs(LogCategory category_mask = LogCategory::All) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (category_mask == LogCategory::All) {
            return m_logs;
        }
        std::vector<LogMessage> filtered_logs;
        for (const auto& log : m_logs) {
            if ((static_cast<uint32_t>(log.category) & static_cast<uint32_t>(category_mask)) != 0) {
                filtered_logs.push_back(log);
            }
        }
        return filtered_logs;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.clear();
    }

private:
    LogHistoryManager() = default;
    static const size_t MAX_LOGS = 200;
    std::vector<LogMessage> m_logs;
    std::mutex m_mutex;
};

// =================== State variables ===================

static HANDLE hConsole = nullptr;
static bool g_enableSingleLinePerKey = true;

static std::mutex g_logStateMutex;
static std::mutex g_logConsoleMutex;

// =================== Helper ghi Console thuần Windows API ===================

static void DirectConsoleWrite(HANDLE hCon, const std::string& text) {
    if (text.empty()) return;
    DWORD written = 0;
    WriteConsoleA(hCon, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
}

// =================== Màu cho từng nhóm tag ===================

static WORD GetLogColor(const LogLevel& lv) {
    if (lv == LogLevel::Critical)   return BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    if (lv == LogLevel::Error)      return FOREGROUND_RED | FOREGROUND_INTENSITY;
    if (lv == LogLevel::Warning)    return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    if (lv == LogLevel::Info)       return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    if (lv == LogLevel::Debug)      return FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; 
}

// =================== Ghi đè có giới hạn (Console Output) ===================

static void RateLimitedLogOverwrite(
    const std::string& key,
    int interval_ms,
    const std::string& message,
    const std::string& location,
    LogLevel level,
    LogCategory category)
{
    struct LogEntry {
        std::chrono::steady_clock::time_point last{};
        std::string lastMessage;
        size_t lastLength = 0;
        bool printedOnce = false;
        DWORD consolePosY = 0;
        uint32_t repeatCount = 1;
    };

    static std::unordered_map<std::string, LogEntry> logs;
    static std::string lastKey;

    bool printedOnce = false;
    DWORD consolePosY = 0;
    size_t lastLength = 0;
    uint32_t currentRepeatCount = 1;

    {
        std::lock_guard<std::mutex> lock(g_logStateMutex);

        auto& entry = logs[key];
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.last).count();

        // Luôn ghi nhận vào history ngay cả khi bị chặn rate-limit
        LogHistoryManager::GetInstance().AddOrUpdateLog(key, message, location, level, category);

        if (elapsed < interval_ms || message == entry.lastMessage) {
            entry.repeatCount++;
            entry.lastMessage = message;
            // Nếu chưa đủ thời gian nén (interval), bỏ qua in console
            if (elapsed < interval_ms) {
                return;
            }
        } else {
            entry.repeatCount = 1;
        }

        entry.last = now;
        entry.lastMessage = message;
        currentRepeatCount = entry.repeatCount;

        printedOnce = entry.printedOnce;
        consolePosY = entry.consolePosY;
        lastLength = entry.lastLength;
    }

    // Định dạng chuỗi hiển thị có kèm thông tin lặp repeatCount
    std::string full_message = "[" + location + "] " + message;
    if (currentRepeatCount > 1) {
        full_message += " (x" + std::to_string(currentRepeatCount) + ")";
    }

    const auto waitStart = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> consoleLock(g_logConsoleMutex);

    const auto acquired = std::chrono::steady_clock::now();
    const auto waitUs = std::chrono::duration_cast<std::chrono::microseconds>(acquired - waitStart).count();

    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == nullptr || hConsole == INVALID_HANDLE_VALUE) {
        return;
    }

    WORD color = GetLogColor(level);
    SetConsoleTextAttribute(hConsole, color);

    if (g_enableSingleLinePerKey) {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        GetConsoleScreenBufferInfo(hConsole, &info);

        if (!printedOnce) {
            DirectConsoleWrite(hConsole, full_message + "\n");
            consolePosY = info.dwCursorPosition.Y;
            printedOnce = true;
        } else {
            COORD pos = { 0, static_cast<SHORT>(consolePosY) };
            SetConsoleCursorPosition(hConsole, pos);

            std::string out = "\r" + full_message;
            if (full_message.size() < lastLength) {
                out.append(lastLength - full_message.size(), ' ');
            }
            DirectConsoleWrite(hConsole, out);
        }
    } else {
        if (key == lastKey) {
            std::string out = "\r" + full_message;
            if (full_message.size() < lastLength) {
                out.append(lastLength - full_message.size(), ' ');
            }
            DirectConsoleWrite(hConsole, out);
        } else {
            std::string out;
            if (!lastKey.empty()) {
                out += "\n";
            }
            out += full_message;
            DirectConsoleWrite(hConsole, out);

            lastKey = key;
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_logStateMutex);

        auto it = logs.find(key);
        if (it != logs.end()) {
            it->second.lastLength = full_message.size();
            it->second.printedOnce = printedOnce;
            it->second.consolePosY = consolePosY;
        }
    }

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    const auto released = std::chrono::steady_clock::now();
    const auto holdUs = std::chrono::duration_cast<std::chrono::microseconds>(released - acquired).count();

    if (waitUs >= 1000 || holdUs >= 5000) {
        char buffer[1024];
        sprintf_s(
            buffer,
            "[LOG-PERF] tid=%lu key=%s wait=%lld us hold=%lld us\n",
            static_cast<unsigned long>(GetCurrentThreadId()),
            key.c_str(),
            static_cast<long long>(waitUs),
            static_cast<long long>(holdUs)
        );
        OutputDebugStringA(buffer);
    }
}

// =================== Macros chính hỗ trợ fmt ===================

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#if defined(_MSC_VER)
    #define LOG_FUNCTION_NAME __FUNCSIG__
#else
    #define LOG_FUNCTION_NAME __PRETTY_FUNCTION__
#endif

#ifdef ENABLE_LOG
#define LOG(interval_ms, level, category, fmt_str, ...) \
    do { \
        static const char* _dynamic_key = __FILE__ ":" STRINGIZE(__LINE__); \
        std::string _log_loc = std::string(__FILE__) + ":" STRINGIZE(__LINE__) + " (" + LOG_FUNCTION_NAME + ")"; \
        std::string _msg_dynamic = FormatString(fmt_str, ##__VA_ARGS__); \
        RateLimitedLogOverwrite(_dynamic_key, interval_ms, _msg_dynamic, _log_loc, level, category); \
    } while (0)
#else
#define LOG(interval_ms, level, category, fmt_str, ...) \
    do { \
    } while (0)
#endif