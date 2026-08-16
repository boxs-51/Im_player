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

    // 1. Chuyển đổi tham số sang dạng an toàn cho snprintf
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
            // Xử lý con trỏ char* bị nullptr
            const char* p = static_cast<const char*>(arg);
            return p ? p : "(null)";
        }
        else {
            return std::forward<T>(arg);
        }
    }

    // 2. Hàm gọi snprintf được bọc trong SEH của Windows để bắt lỗi Access Violation tại runtime
    inline int SafeSnprintf(char* buf, size_t count, const char* fmt, ...) {
        int result = -1;
        va_list args;
        va_start(args, fmt);

        __try {
            result = vsnprintf(buf, count, fmt, args);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // Bắt lỗi Access Violation (0xC0000005) nếu truyền sai định dạng %s với kiểu số
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
        // Lần 1: Xác định độ dài chuỗi bằng SEH wrapper
        int size_s = Detail::SafeSnprintf(nullptr, 0, fmt, Detail::ConvertArg(std::forward<Args>(args))...);
        
        // Nếu SEH bắt được lỗi crash do sai định dạng %s với kiểu dữ liệu
        if (size_s == -2) {
            char errBuf[512];
            sprintf_s(errBuf, "[LOG FMT ERROR] Bad format specifier or type mismatch in fmt: \"%s\"", fmt);
            OutputDebugStringA(errBuf);
            return std::string(errBuf);
        }

        if (size_s <= 0) return std::string();

        size_t size = static_cast<size_t>(size_s);
        std::string buf(size, '\0');
        
        // Lần 2: Ghi dữ liệu thực tế
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
    std::string message;
    std::string key;
    LogLevel level;
    LogCategory category;
};

class LogHistoryManager {
public:
    static LogHistoryManager& GetInstance() {
        static LogHistoryManager instance;
        return instance;
    }

    void AddLog(
        const std::string& key,
        const std::string& message,
        LogLevel level = LogLevel::Info,
        LogCategory category = LogCategory::System)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_logs.push_back({
            std::chrono::system_clock::now(),
            message,
            key,
            level,
            category
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
static bool g_enableSingleLinePerKey = false;

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
    LogLevel level,
    LogCategory category)
{
    struct LogEntry {
        std::chrono::steady_clock::time_point last{};
        std::string lastMessage;
        size_t lastLength = 0;
        bool printedOnce = false;
        DWORD consolePosY = 0;
    };

    static std::unordered_map<std::string, LogEntry> logs;
    static std::string lastKey;

    bool printedOnce = false;
    DWORD consolePosY = 0;
    size_t lastLength = 0;

    {
        std::lock_guard<std::mutex> lock(g_logStateMutex);

        auto& entry = logs[key];

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.last).count();

        if (elapsed < interval_ms || message == entry.lastMessage) {
            return;
        }

        entry.last = now;
        entry.lastMessage = message;

        printedOnce = entry.printedOnce;
        consolePosY = entry.consolePosY;
        lastLength = entry.lastLength;
    }

    LogHistoryManager::GetInstance().AddLog(key, message, level, category);

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
            DirectConsoleWrite(hConsole, message + "\n");
            consolePosY = info.dwCursorPosition.Y;
            printedOnce = true;
        } else {
            COORD pos = { 0, static_cast<SHORT>(consolePosY) };
            SetConsoleCursorPosition(hConsole, pos);

            std::string out = "\r" + message;
            if (message.size() < lastLength) {
                out.append(lastLength - message.size(), ' ');
            }
            DirectConsoleWrite(hConsole, out);
        }
    } else {
        if (key == lastKey) {
            std::string out = "\r" + message;
            if (message.size() < lastLength) {
                out.append(lastLength - message.size(), ' ');
            }
            DirectConsoleWrite(hConsole, out);
        } else {
            std::string out;
            if (!lastKey.empty()) {
                out += "\n";
            }
            out += message;
            DirectConsoleWrite(hConsole, out);

            lastKey = key;
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_logStateMutex);

        auto it = logs.find(key);
        if (it != logs.end()) {
            it->second.lastLength = message.size();
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

#define LOG(key, interval_ms, level, category, fmt_str, ...) \
    do { \
        std::string _msg_##key = FormatString(fmt_str, ##__VA_ARGS__); \
        RateLimitedLogOverwrite(#key, interval_ms, _msg_##key, level, category); \
    } while (0)

#define LOG_NO_KEY(interval_ms, level, category, fmt_str, ...) \
    do { \
        static const char* _dynamic_key = __FILE__ ":" STRINGIZE(__LINE__); \
        std::string _msg_dynamic = FormatString(fmt_str, ##__VA_ARGS__); \
        RateLimitedLogOverwrite(_dynamic_key, interval_ms, _msg_dynamic, level, category); \
    } while (0)
    
#include <unordered_set>
// =========================================================
// 🔹 Globals
// ==========================================================
static std::unordered_map<std::string, std::vector<std::string>> g_ErrorGroups; // key = level ("warn", "error")
static std::mutex g_ErrorMutex;
static std::condition_variable g_ErrorCv;
static std::atomic<bool> g_ErrorThreadRunning(false);
static std::unordered_set<std::string> g_CurrentBatchErrors;
static bool g_EnableWarnLogs = false; // ✅ Cho phép bật/tắt log cảnh báo
static bool g_EnableLog = false;

typedef int (WINAPI *MessageBoxTimeoutA_t)(
    HWND hWnd,
    LPCSTR lpText,
    LPCSTR lpCaption,
    UINT uType,
    WORD wLanguageId,
    DWORD dwMilliseconds
);
static int MessageBoxTimeoutA(
    HWND hWnd,
    const char* text,
    const char* caption,
    UINT type,
    DWORD timeoutMs
) {
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (!user32) return 0;

    auto func = (MessageBoxTimeoutA_t)GetProcAddress(
        user32,
        "MessageBoxTimeoutA"
    );
    if (!func) return 0;

    return func(hWnd, text, caption, type, 0, timeoutMs);
}
// ==========================================================
// 🔹 Thread xử lý lỗi MPV theo level
// ==========================================================
static void MpvErrorThreadFunc() {
    std::unordered_map<std::string, std::vector<std::string>> batch;

    {
        std::lock_guard<std::mutex> lock(g_ErrorMutex);
        batch = std::move(g_ErrorGroups);
        g_ErrorGroups.clear();
    }

    // Hiển thị popup và log
    for (const auto& pair : batch) {
        const std::string& level = pair.first;
        const auto& msgs = pair.second;
        if (msgs.empty()) continue;

        std::string combined;
        for (const auto& msg : msgs) {
            combined += msg + "\n\n";

            if (level == "warn") {
                LOG(mpv_warn_error, 1, LogLevel::Warning, LogCategory::Network,
                    "[DEBUG] [WARNING] %s", msg );
            } 
            else if (level == "error") {
                LOG(mpv_error_error, 1, LogLevel::Error, LogCategory::Network,
                    "[DEBUG] [ERROR] %s", msg );
            } 
            else {
                "[ << %s << ] ", msg;
            }
        }
        // ✅ Bỏ qua nếu đang tắt log cảnh báo
        if ((level == "warn" && !g_EnableWarnLogs) || !g_EnableLog)
            continue;

        std::string levelUpper = level;
        std::transform(levelUpper.begin(), levelUpper.end(), levelUpper.begin(),
                    [](unsigned char c){ return std::toupper(c); });

        MessageBoxTimeoutA(
            NULL,
            combined.c_str(),
            ("MPV " + levelUpper + " MESSAGE").c_str(),
            MB_OK | MB_ICONERROR,
            5000 // timeout 5 giây
        );
    }

    // ✅ Sau khi xử lý xong batch → xóa toàn bộ lỗi đã log
    {
        std::lock_guard<std::mutex> lock(g_ErrorMutex);
        g_CurrentBatchErrors.clear();
        g_ErrorGroups.clear();
    }

    g_ErrorThreadRunning = false;
}

// ==========================================================
// 🔹 Push lỗi MPV trực tiếp theo level
// ==========================================================
inline void PushMpvError(const char* level, const char* text) {
    if (!level || !text) return;
    std::string lvl(level);
    std::string msg(text);

    std::lock_guard<std::mutex> lock(g_ErrorMutex);

    // ✅ Nếu lỗi này đã được push trong batch hiện tại → bỏ qua
    if (g_CurrentBatchErrors.find(msg) != g_CurrentBatchErrors.end())
        return;

    // ✅ Thêm lỗi mới
    g_CurrentBatchErrors.insert(msg);
    g_ErrorGroups[lvl].push_back(msg);

    // ✅ Nếu thread chưa chạy → khởi động xử lý
    if (!g_ErrorThreadRunning) {
        g_ErrorThreadRunning = true;
        std::thread(MpvErrorThreadFunc).detach();
    }
}

// ==========================================================
// 🔹 Reset batch lỗi hiện tại (nếu muốn thủ công)
// ==========================================================
inline void ResetMpvErrorBatch() {
    std::lock_guard<std::mutex> lock(g_ErrorMutex);
    g_CurrentBatchErrors.clear();
    g_ErrorGroups.clear();
}

// ==========================================================
// 🔹 Bật / Tắt log cảnh báo (tùy chọn)
// ==========================================================
inline void SetWarnLoggingEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(g_ErrorMutex);
    g_EnableWarnLogs = enabled;
}
inline void SetLoggingEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(g_ErrorMutex);
    g_EnableLog = enabled;
}
