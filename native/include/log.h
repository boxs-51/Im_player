#include "globals.h"
#include <iostream>
#include <chrono>
#include <string>
#include <sstream>
#include <windows.h>
#include <windows.h>
#include <cstdio>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include <iomanip>


static HANDLE hConsole = nullptr;
static bool g_consoleAttached = false;
static bool g_consoleWindowCreated = false;
static bool g_blockConsoleClose = true;
static bool g_enableSingleLinePerKey = false;

// =================== Màu cho từng nhóm tag ===================
static WORD GetLogColor(const std::string& text) {
    if (text.find("[ERROR]")                != std::string::npos) return FOREGROUND_RED   | FOREGROUND_INTENSITY;
    if (text.find("Error")                  != std::string::npos) return FOREGROUND_RED   | FOREGROUND_INTENSITY;
    if (text.find("[WARNING]")              != std::string::npos) return FOREGROUND_RED   | FOREGROUND_GREEN        |FOREGROUND_INTENSITY;
    if (text.find("Warning")                != std::string::npos) return FOREGROUND_RED   | FOREGROUND_GREEN        |FOREGROUND_INTENSITY;
    if (text.find("[INFO]")                 != std::string::npos) return FOREGROUND_GREEN | FOREGROUND_BLUE         | FOREGROUND_INTENSITY;
    if (text.find("Info")                   != std::string::npos) return FOREGROUND_GREEN | FOREGROUND_BLUE         | FOREGROUND_INTENSITY;
    //if (text.find("[DEBUG]")                != std::string::npos) return FOREGROUND_BLUE  | FOREGROUND_INTENSITY;
    //if (text.find("Debug")                  != std::string::npos) return FOREGROUND_BLUE  | FOREGROUND_INTENSITY;
    return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // trắng mặc định
}

// =================== Ghi đè có giới hạn ===================
static void RateLimitedLogOverwrite(const std::string& key, int interval_ms, const std::string& message)
{
    struct LogEntry {
        std::chrono::steady_clock::time_point last;
        std::string lastMessage;
        size_t lastLength = 0;
        bool printedOnce = false;
        DWORD consolePosY = 0;
        int linesNeeded = 1;
    };

    static std::unordered_map<std::string, LogEntry> logs;
    static std::vector<std::string> g_keyOrder;
    static std::string lastKey;
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    auto& entry = logs[key];
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.last).count();
    if (elapsed < interval_ms || message == entry.lastMessage)
        return;

    entry.last = now;
    entry.lastMessage = message;

    WORD color = GetLogColor(message);
    SetConsoleTextAttribute(hConsole, color);

    // =============================
    // 🔀 Hai chế độ hiển thị
    // =============================
    if (g_enableSingleLinePerKey)
    {
        // --- Chế độ "mỗi key = 1 dòng" ---
        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(hConsole, &info);

        if (!entry.printedOnce)
        {
            std::cout << message << std::endl;
            entry.consolePosY = info.dwCursorPosition.Y; // lưu dòng hiện tại
            entry.printedOnce = true;
        }
        else
        {
            COORD pos = { 0, static_cast<SHORT>(entry.consolePosY) };
            SetConsoleCursorPosition(hConsole, pos);
            std::cout << "\r" << message;

            if (message.size() < entry.lastLength)
                std::cout << std::string(entry.lastLength - message.size(), ' ');

            std::cout << std::flush;
        }
    }
    else
    {
        // --- Chế độ ghi đè kiểu cũ ---
        if (key == lastKey)
        {
            std::cout << "\r" << message;
            if (message.size() < entry.lastLength)
                std::cout << std::string(entry.lastLength - message.size(), ' ');
            std::cout << std::flush;
        }
        else
        {
            if (!lastKey.empty()) std::cout << std::endl;
            std::cout << message << std::flush;
            lastKey = key;
        }
    }

    entry.lastLength = message.size();
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

// =================== Helper ghi song song ra 2 stream ===================
template<typename F>
std::string CaptureAndMirror(F&& func)
{
    std::ostringstream oss;
    auto old_buf = std::cout.rdbuf(oss.rdbuf());  // redirect cout tạm thời sang oss
    func();                                       // chạy biểu thức std::cout << ...
    std::cout.rdbuf(old_buf);                     // khôi phục lại cout
    return oss.str();
}

// =================== Macro chính ===================
#ifndef RATE_LIMITED_COUT
#define RATE_LIMITED_COUT(key, interval_ms, expr)                             \
    do {                                                                      \
        std::string _msg_##key = CaptureAndMirror([&]() { expr; });           \
        RateLimitedLogOverwrite(#key, interval_ms, _msg_##key);               \
    } while (0)
#endif

// ------------------------- Ctrl Handler -------------------------
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_CLOSE_EVENT:
        if (g_blockConsoleClose)
        {
            RATE_LIMITED_COUT(system_console_close_blocked, 1,std::cout << "[WARNING] [System] Console close event blocked. Use command or hotkey to close.");
            return TRUE;
        }
        break;

    case CTRL_C_EVENT:
        RATE_LIMITED_COUT(system_ctrl_c_pressed, 1,std::cout << "[WARNING] [System] Ctrl+C pressed — ignored (app still running)");
        return TRUE;

    default:
        break;
    }
    return FALSE;
}

// ------------------------- Config Output/Input -------------------------
static void ConfigureConsoleOutputMode()
{
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;

    DWORD mode = 0;
    if (GetConsoleMode(hConsole, &mode))
    {
        mode |= ENABLE_PROCESSED_OUTPUT             |//Xử lý các ký tự điều khiển như Ctrl+C, LF, CR, Backspace.
                ENABLE_WRAP_AT_EOL_OUTPUT           |//Khi tới cuối dòng, tự động xuống dòng.
                ENABLE_VIRTUAL_TERMINAL_PROCESSING  |//Cho phép ANSI escape sequences (màu sắc, cursor control, text formatting).
                DISABLE_NEWLINE_AUTO_RETURN         |//Dòng mới (LF) không ép con trỏ về cột 0.
                ENABLE_LVB_GRID_WORLDWIDE;           //Hỗ trợ hiển thị các ký tự double-byte, grid characters quốc tế.


        SetConsoleMode(hConsole, mode);
    }
}

static void ConfigureConsoleInputMode()
{
    hConsole = GetStdHandle(STD_INPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;

    DWORD mode = 0;
    if (GetConsoleMode(hConsole, &mode))
    {
        mode |= ENABLE_EXTENDED_FLAGS   | //Cho phép cấu hình nâng cao.
                ENABLE_PROCESSED_INPUT  | //Xử lý các ký tự điều khiển như Ctrl+C.
                ENABLE_WINDOW_INPUT     | //Bắt sự kiện thay đổi kích thước cửa sổ console.
                ENABLE_QUICK_EDIT_MODE  | //Cho phép người dùng chọn văn bản trong console (nhưng có thể gây freeze).
                ENABLE_MOUSE_INPUT      | //Bắt sự kiện chuột.
                ENABLE_AUTO_POSITION;     //Tự động đặt con trỏ vào vị trí nhập liệu.

        SetConsoleMode(hConsole, mode);
    }
}

// ------------------------- Thử attach vào console cha -------------------------
static bool TryAttachToParentConsole()
{
    // Nếu đã có console (VD: chạy từ cmd hoặc VS terminal)
    if (GetConsoleWindow() != nullptr)
        return true;

    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        FILE* fDummy;
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
        freopen_s(&fDummy, "CONIN$",  "r", stdin);
        std::ios::sync_with_stdio(true);
        std::cout.clear();
        std::cerr.clear();
        ConfigureConsoleOutputMode();
        ConfigureConsoleInputMode();
        return true;
    }
    return false;
}

// ================================================================
// ✅ GIAI ĐOẠN 1: KHỞI TẠO
// ================================================================
inline void InitConsoleSystem()
{
    if (TryAttachToParentConsole())
    {
        g_consoleAttached = true;
        g_consoleWindowCreated = false;
        RATE_LIMITED_COUT(system_console_attached, 1,std::cout << "[DEBUG] [WARNING] [System] Attached to parent console.\n");
    }
    else
    {
        g_consoleAttached = false;
        g_consoleWindowCreated = false;
        RATE_LIMITED_COUT(system_console_not_attached, 1,std::cout << "[DEBUG] [WARNING] [System] No parent console found. Console output disabled.");
    }

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
}

// ================================================================
// ✅ GIAI ĐOẠN 2: MỞ / TẮT CONSOLE RIÊNG
// ================================================================
inline bool OpenConsoleWindow()
{
    if (g_consoleAttached || g_consoleWindowCreated){
        std::cout << "[System] Console window has been initialized or Attached \n";
        return false;
    }

    if (!AllocConsole())
        return false;

    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$",  "r", stdin);

    std::ios::sync_with_stdio(true);
    std::cout.clear();
    std::cerr.clear();

    // ✅ phải gọi sau freopen
    ConfigureConsoleOutputMode();
    ConfigureConsoleInputMode();

    SetConsoleTitleA("Debug Console");

    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE)
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    
    std::cout << "======================== Debug Console Created ======================\n";
    
    g_consoleWindowCreated = true;
    return true;
}

inline bool CloseConsoleWindow()
{
    if (g_consoleAttached)
    {
        std::cout << "[System] Cannot close attached console (parent terminal)\n";
        return false;
    }

    if (!g_consoleWindowCreated)
    {
        std::cout << "[System] Console Windown Cannot created \n";
        return false;
    }

    std::cout << "======================== Debug Console Closed ======================\n";
    std::cout.flush();
    FreeConsole();
    g_consoleWindowCreated = false;
    return true;
}

// ================================================================
// ✅ TRẠNG THÁI
// ================================================================
inline bool IsConsoleVisible()
{
    return (g_consoleAttached || g_consoleWindowCreated);
}

// =========================================================
// 🔹 Globals
// ==========================================================
static std::unordered_map<std::string, std::vector<std::string>> g_ErrorGroups; // key = level ("warn", "error")
static std::mutex g_ErrorMutex;
static std::condition_variable g_ErrorCv;
static std::atomic<bool> g_ErrorThreadRunning(false);
static std::unordered_set<std::string> g_CurrentBatchErrors;
static bool g_EnableWarnLogs = false; // ✅ Cho phép bật/tắt log cảnh báo

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
                RATE_LIMITED_COUT(mpv_warn_error, 1,
                    std::cout << "[DEBUG] [WARNING] " << msg << std::endl);
            } 
            else if (level == "error") {
                RATE_LIMITED_COUT(mpv_error_error, 1,
                    std::cout << "[DEBUG] [ERROR] " << msg << std::endl);
            } 
            else {
                std::cout << "[" << level << "] " << msg << std::endl;
            }
        }
        // ✅ Bỏ qua nếu đang tắt log cảnh báo
        if (level == "warn" && !g_EnableWarnLogs)
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
