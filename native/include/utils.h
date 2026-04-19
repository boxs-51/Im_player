#ifndef UTILS_H
#define UTILS_H

#pragma once

#include <mpv/mpv_settings.h>

#include <windows/windows_borderless_state.h>
#include <util.h>
#include <string>
#include <vector>
#include <SDL.h>
#include <imgui.h>
#include <mpv/client.h>
#include "reusable_popup.h"
#include <fstream>  
#include <locale>  
#include <windows.h>
#include <filesystem>
#include <windowsx.h>
#include <map>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <type_traits>
#include <initializer_list>
#include <stdint.h>
namespace fs = std::filesystem;
typedef unsigned int Uint;
// Lưu ý: Không có khoảng trắng giữa COL_32 và (
#define COL_32(r,g,b,a) ((Uint)(((a)&255)<<24) | (((b)&255)<<16) | (((g)&255)<<8) | ((r)&255))

struct Vec2 {
    float x, y;
    Vec2() = default;
    Vec2(float _x, float _y) : x(_x), y(_y) {} 
};
struct Vec4 {
    float x, y, w, h;
    Vec4() = default;
    Vec4(float _x, float _y, float _w, float _h) : x(_x), y(_y), w(_w), h(_h) {}
};

struct Col32 {
    Uint r ,b ,g ,a;
    Col32() = default;
    Col32(float _r, float _b, float _g, float _a) : r(_r), b(_b), g(_g), a(_a) {}
};

inline ImU32 ToCol32(const ImVec4& c) {return IM_COL32((Uint)(c.x*255.0f), (Uint)(c.y*255.0f), (Uint)(c.z*255.0f), (Uint)(c.w*255.0f));}
//inline Col32 ToCol32 (const Col32& v) {return COL_32((Uint)v.r, (Uint)v.b, (Uint)v.g, (Uint)v.a);}
//inline Col32 ToCol32 (const Vec4& v) {return COL_32((Uint)v.x ,(Uint)v.y ,(Uint)v.w ,(Uint)v.h);}
inline ImVec2 ToImVec2 (const Vec2& v) {return ImVec2{v.x, v.y};}
inline ImVec4 ToImVec4 (const Vec4& v) {return ImVec4{v.x, v.y, v.w, v.h};}
inline Vec2 ToVec2 (const ImVec2& v) {return Vec2{v.x, v.y};}
inline Vec4 ToVec4 (const ImVec4& v) {return Vec4{v.x, v.y, v.z, v.w};}

struct CardHoleStyle
{
    float rounding = 6.0f;
    float borderThickness = 1.5f;
};

struct WindowContext {
    SDL_Window* mainWindow = nullptr;
    SDL_GLContext mainGLContext = nullptr;
    ImGuiContext* mainImGuiCtx = nullptr;

};

struct WindowLayout {

    int WinW;
    int WinH;
    int WinX;
    int WinY;

    SDL_Rect titleBar;   // Vùng titlebar
    SDL_Rect videoArea;  // Vùng video/content

    Vec2 DisplaySize;

    Vec2 VideoPos;
    Vec2 VideoSize;

    Vec2 WinDowPos;
    Vec2 WinDowSize;

    Vec2 TitlePos;
    Vec2 TitleSize;

};

// --- Đối với ImVec2 ---

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
inline ImVec2 operator/(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x / rhs.x, lhs.y / rhs.y); }

inline ImVec2 operator*(const ImVec2& lhs, float scalar) {return ImVec2{lhs.x * scalar, lhs.y * scalar}; } 
inline ImVec2 operator*(float scalar, const ImVec2& lhs) {return ImVec2{lhs.x * scalar, lhs.y * scalar}; }
    
inline ImVec2 operator/(const ImVec2& lhs, float scalar) {return ImVec2{lhs.x / scalar, lhs.y / scalar}; } 
inline ImVec2 operator/(float scalar, const ImVec2& lhs) {return ImVec2{lhs.x / scalar, lhs.y / scalar}; }

// Toán tử gán phải thay đổi lhs và trả về tham chiếu
inline ImVec2& operator+=(ImVec2& lhs, float scalar) { lhs.x += scalar; lhs.y += scalar; return lhs; }
inline ImVec2& operator-=(ImVec2& lhs, float scalar) { lhs.x -= scalar; lhs.y -= scalar; return lhs; }
inline ImVec2& operator*=(ImVec2& lhs, float scalar) { lhs.x *= scalar; lhs.y *= scalar; return lhs; }
inline ImVec2& operator/=(ImVec2& lhs, float scalar) { lhs.x /= scalar; lhs.y /= scalar; return lhs; }

// --- Đối với Vec2 ---

inline Vec2 operator+(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x + rhs.x, lhs.y + rhs.y); }
inline Vec2 operator-(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x - rhs.x, lhs.y - rhs.y); }
inline Vec2 operator*(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x * rhs.x, lhs.y * rhs.y); }
inline Vec2 operator/(const Vec2& lhs, const Vec2& rhs) { return Vec2(lhs.x / rhs.x, lhs.y / rhs.y); }

inline Vec2 operator*(const Vec2& lhs, float scalar) {return Vec2{lhs.x * scalar, lhs.y * scalar}; } 
inline Vec2 operator*(float scalar, const Vec2& lhs) {return Vec2{lhs.x * scalar, lhs.y * scalar}; }
    
inline Vec2 operator/(const Vec2& lhs, float scalar) {return Vec2{lhs.x / scalar, lhs.y / scalar}; } 
inline Vec2 operator/(float scalar, const Vec2& lhs) {return Vec2{lhs.x / scalar, lhs.y / scalar}; }

inline Vec2& operator+=(Vec2& lhs, float scalar) { lhs.x += scalar; lhs.y += scalar; return lhs; }
inline Vec2& operator-=(Vec2& lhs, float scalar) { lhs.x -= scalar; lhs.y -= scalar; return lhs; }
inline Vec2& operator*=(Vec2& lhs, float scalar) { lhs.x *= scalar; lhs.y *= scalar; return lhs; }
inline Vec2& operator/=(Vec2& lhs, float scalar) { lhs.x /= scalar; lhs.y /= scalar; return lhs; }

// Toán tử số học cơ bản
inline ImVec4 operator+(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w); }
inline ImVec4 operator-(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w); }

// Toán tử gán (Sửa lỗi logic: thay đổi trực tiếp lhs và trả về tham chiếu)
inline ImVec4& operator+=(ImVec4& lhs, float scalar) { lhs.x += scalar; lhs.y += scalar; lhs.z += scalar; lhs.w += scalar; return lhs; }
inline ImVec4& operator-=(ImVec4& lhs, float scalar) { lhs.x -= scalar; lhs.y -= scalar; lhs.z -= scalar; lhs.w -= scalar; return lhs; }
inline ImVec4& operator*=(ImVec4& lhs, float scalar) { lhs.x *= scalar; lhs.y *= scalar; lhs.z *= scalar; lhs.w *= scalar; return lhs; }
inline ImVec4& operator/=(ImVec4& lhs, float scalar) { lhs.x /= scalar; lhs.y /= scalar; lhs.z /= scalar; lhs.w /= scalar; return lhs; }

// Toán tử số học cơ bản
inline Vec4 operator+(const Vec4& lhs, const Vec4& rhs) { return Vec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.w + rhs.w, lhs.h + rhs.h); }
inline Vec4 operator-(const Vec4& lhs, const Vec4& rhs) { return Vec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.w - rhs.w, lhs.h - rhs.h); }
inline Vec4 operator*(const Vec4& lhs, const Vec4& rhs) { return Vec4(lhs.x * rhs.x, lhs.y * rhs.y, lhs.w * rhs.w, lhs.h * rhs.h); }
inline Vec4 operator/(const Vec4& lhs, const Vec4& rhs) { return Vec4(lhs.x / rhs.x, lhs.y / rhs.y, lhs.w / rhs.w, lhs.h / rhs.h); }

// Toán tử gán
inline Vec4& operator+=(Vec4& lhs, float scalar) { lhs.x += scalar; lhs.y += scalar; lhs.w += scalar; lhs.h += scalar; return lhs; }
inline Vec4& operator-=(Vec4& lhs, float scalar) { lhs.x -= scalar; lhs.y -= scalar; lhs.w -= scalar; lhs.h -= scalar; return lhs; }
inline Vec4& operator*=(Vec4& lhs, float scalar) { lhs.x *= scalar; lhs.y *= scalar; lhs.w *= scalar; lhs.h *= scalar; return lhs; }
inline Vec4& operator/=(Vec4& lhs, float scalar) { lhs.x /= scalar; lhs.y /= scalar; lhs.w /= scalar; lhs.h /= scalar; return lhs; }

extern WindowLayout Windowlayout;
extern WindowContext ctx;

void UpdateHoverAnim(float& animValue, bool isHovering, float speed = 12.0f);

void ApplyDynamicMPVConfig(mpv_handle* mpv);
void ApplyStaticMPVConfig(mpv_handle* mpv);
void LoadAllScripts(mpv_handle* mpv);
void UpdateGlobalWindowLayout(SDL_Window* sdlWindow, DragResizeState& state , WindowLayout& w);
void TerminateHandler();
void SignalHandler(int signal);
void NotifyActivity(bool& show_ui_video);
bool SetDelayHover( bool isHovering, double delaySeconds = 3.0, const char * id = nullptr ) ;


std::wstring UTF8ToWide(const std::string& str);
std::string WideToUTF8(const std::wstring& wstr);

template<typename T>
bool mpv_get_prop(mpv_handle* mpv, const std::string& name, T& out);

// Specialization for double
template<>
inline bool mpv_get_prop<double>(mpv_handle* mpv, const std::string& name, double& out) {
    return mpv_get_property(mpv, name.c_str(),MPV_FORMAT_DOUBLE, &out) >= 0;
}

// Specialization for int
template<>
inline bool mpv_get_prop<int>(mpv_handle* mpv, const std::string& name, int& out) {
    return mpv_get_property(mpv, name.c_str(),MPV_FORMAT_INT64, &out) >= 0;
}

// Specialization for bool
template<>
inline bool mpv_get_prop<bool>(mpv_handle* mpv, const std::string& name, bool& out) {
    int i=0;
    bool res = mpv_get_property(mpv, name.c_str(),MPV_FORMAT_FLAG, &i) >= 0;
    out = (i != 0);
    return res;
}

// Specialization for const char*
template<>
inline bool mpv_get_prop<const char*>(mpv_handle* mpv, const std::string& name, const char*& out) {

    return mpv_get_property(mpv, name.c_str(),MPV_FORMAT_STRING, &out) >= 0;
}

// Specialization for std::string
template<>
inline bool mpv_get_prop<std::string>(mpv_handle* mpv, const std::string& name, std::string& out) {
    const char* tmp = nullptr;
    int ret = mpv_get_property(mpv, name.c_str(), MPV_FORMAT_STRING, &tmp);
    if (ret != 0) return false; // 0 = success

    out = tmp ? tmp : "";       // copy vào std::string
    if (tmp) mpv_free(const_cast<void*>(reinterpret_cast<const void*>(tmp)));
    return true;
}

inline ImVec4 ToImVec2(SDL_Rect &r) {return ImVec4((float) r.x, (float)r.y, (float)(r.x + r.w), (float)(r.y + r.h));}
inline ImVec4 ToImVec4_RAW(SDL_Rect &r) {return ImVec4((float)r.x, (float)r.y, (float)r.w, (float)r.h);}
inline ImVec2 ToImVec2_Pos(SDL_Rect &r) {return ImVec2((float)r.x, (float)r.y);}
inline ImVec2 ToImVec2_Size(SDL_Rect &r) {return ImVec2((float)r.w, (float)r.h);}

inline Vec4 ToVec4(SDL_Rect &r) {return Vec4((float) r.x, (float)r.y, (float)(r.x + r.w), (float)(r.y + r.h));}
inline Vec4 ToVec4_RAW(SDL_Rect &r) {return Vec4((float)r.x, (float)r.y, (float)r.w, (float)r.h);}
inline Vec2 ToVec2_Pos(SDL_Rect &r) {return Vec2((float)r.x, (float)r.y);}
inline Vec2 ToVec2_Size(SDL_Rect &r) {return Vec2((float)r.w, (float)r.h);}


// === Chuyển đổi sang string UTF-8/UTF-16 ===
inline std::string ToUtf8(const std::filesystem::path& p) { return p.u8string(); }
inline std::string ToUtf8(const std::string& s) { return s; }
inline std::string ToUtf8(const char* s) { return std::string(s); }
inline std::string ToUtf8(const std::wstring& ws) { return std::filesystem::path(ws).u8string(); }
inline std::string ToUtf8(const wchar_t* ws) { return std::filesystem::path(ws).u8string(); }

inline std::wstring ToWString(const std::filesystem::path& p) { return p.wstring(); }
inline std::wstring ToWString(const std::wstring& ws) { return ws; }
inline std::wstring ToWString(const char* s) { return std::filesystem::path(s).wstring(); }
inline std::wstring ToWString(const std::string& s) { return std::filesystem::path(s).wstring(); }

// === Lấy thư mục exe (cached) ===
inline const std::filesystem::path& GetExeDir() {
    static std::filesystem::path cached;
    if (cached.empty()) {
        wchar_t buf[MAX_PATH];
        DWORD len = GetModuleFileNameW(NULL, buf, MAX_PATH);
        if (len == 0 || len == MAX_PATH)
            throw std::runtime_error("GetModuleFileNameW failed");
        cached = std::filesystem::path(buf).parent_path();
    }
    return cached;
}

inline std::filesystem::path FindAppRoot(
    const std::filesystem::path& exeDir,
    int maxUpLevels = -1   // -1 = unlimited (default)
) {
    std::filesystem::path cur = exeDir;
    int level = 0;

    while (!cur.empty())
    {
        if (std::filesystem::exists(cur / "PROJECT_ROOT_1.dat"))
            return cur;

        if (!cur.has_parent_path())
            break;

        if (maxUpLevels >= 0 && level >= maxUpLevels)
            break;

        cur = cur.parent_path();
        ++level;
    }

    return exeDir; // fallback
}
inline const std::filesystem::path& GetAppRoot()
{
    static std::filesystem::path cached =
        FindAppRoot(GetExeDir(), 2);
    return cached;
}
inline void ReplaceAll(
    std::string& s,
    std::string_view from,
    const std::string& to)
{
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}
// === Hàm AutoPath v4 (variadic, type deduction) ===
template<typename T = void, typename... Args>
auto AutoPath(Args&&... args) {
    std::filesystem::path result;

    // ✅ Ghép các phần path an toàn
    auto append_path = [&](const std::filesystem::path& p) {
        if (p.is_absolute())
            result = p;
        else
            result /= p;
    };

    (void)std::initializer_list<int>{
        (append_path(std::filesystem::u8path(ToUtf8(std::forward<Args>(args)))), 0)...
    };

    // ✅ Xử lý placeholder %EXE%
    std::string combined;
    (void)std::initializer_list<int>{
        (combined += ToUtf8(std::forward<Args>(args)) + "/", 0)...
    };

    ReplaceAll(combined, "%EXE%",  GetExeDir().u8string());
    ReplaceAll(combined, "%ROOT%", GetAppRoot().u8string());
    std::filesystem::path final = std::filesystem::u8path(combined);

    // ✅ Nếu là relative path, ghép với thư mục exe
    if (!final.is_absolute())
        final = GetAppRoot() / final;

    final = final.lexically_normal();

    //✅ Nếu path kết thúc bằng separator → coi là dir → strip
    if (!final.empty() && final.filename().empty())
    {
        final = final.parent_path();
    }

    // ✅ Trả kiểu tương ứng
    if constexpr (!std::is_void_v<T>) {
        if constexpr (std::is_same_v<T, std::filesystem::path>)
            return final;
        else if constexpr (std::is_same_v<T, std::string>)
            return final.u8string();
        else if constexpr (std::is_same_v<T, std::wstring>)
            return final.wstring();
        else
            static_assert(!sizeof(T*), "Unsupported type for AutoPath");
    } else {
        // type deduction: trả std::filesystem::path mặc định, có thể ép khi gán
        return final;
    }
}
namespace TextUtils {
    inline std::string TruncateText(const std::string& text, size_t maxLen = 60) {
        if (text.length() <= maxLen) return text;
        return text.substr(0, maxLen) + "...";
    }
    inline std::string TruncateTextByPixels(const char* text, float max_width) {
        std::string s = text;
        ImVec2 size = ImGui::CalcTextSize(s.c_str());
        if (size.x <= max_width) return s;

        // Cắt dần cho đến khi vừa
        while (!s.empty() && ImGui::CalcTextSize((s + "...").c_str()).x > max_width) {
            s.pop_back();
        }
        return s + "...";
    }
    inline std::string TruncateToWidth(const char* text, float max_pixel_width) {
        std::string s = text;
        if (ImGui::CalcTextSize(s.c_str()).x <= max_pixel_width) return s;

        while (!s.empty() && ImGui::CalcTextSize((s + "...").c_str()).x > max_pixel_width) {
            s.pop_back();
        }
        return s + "...";
    }
    inline std::string Format(const char* format, ...) {
        va_list args;
        va_start(args, format);
        // Lấy kích thước cần thiết (không bao gồm ký tự null)
        int size = vsnprintf(nullptr, 0, format, args);
        va_end(args);

        if (size <= 0) return "";

        std::vector<char> buf(size + 1);
        va_start(args, format);
        vsnprintf(buf.data(), buf.size(), format, args);
        va_end(args);

        return std::string(buf.data());
    }
}


#ifndef FRAME_TIMER_H
#define FRAME_TIMER_H

#include <chrono>
#include <thread>
#include <cmath>
// Tự động nhận diện SDL nếu header SDL.h đã được include trước đó
#ifdef SDL_H_
    #define HAS_SDL
#endif

class FrameTimer {
public:
    FrameTimer(int targetFPS = 30) {
        setTargetFPS(targetFPS);
        auto now = std::chrono::high_resolution_clock::now();
        lastFrameTime = now;
        fpsTimestamp = now;
        startPoint = now;
    }

    void setTargetFPS(int fps = 30) {
        targetFPS = fps;
        // Tính toán độ trễ mục tiêu dưới dạng microseconds
        frameDelay = std::chrono::microseconds(1000000 / (targetFPS > 0 ? targetFPS : 1));
    }


    void startFrame() {
        startPoint = std::chrono::high_resolution_clock::now();
    }

    void endFrame() {
        auto endPoint = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endPoint - startPoint);

        // Khống chế FPS
        if (duration < frameDelay) {
            auto sleepTime = frameDelay - duration;
            
#ifdef HAS_SDL
            // SDL_Delay sử dụng milliseconds. 
            // Ta cộng thêm 0.5 để làm tròn thay vì cắt cụt khi ép kiểu.
            SDL_Delay(static_cast<uint32_t>(sleepTime.count() / 1000));
#else
            // C++ Standard sleep
            std::this_thread::sleep_for(sleepTime);
#endif
        }

        // Tính Delta Time (thời gian thực tế giữa 2 lần kết thúc frame)
        auto now = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        if (transition_timer > 0.0f) {
            transition_timer -= deltaTime;
            if (transition_timer <= 0.0f) {
                every_frame = pending_every_frame;
            }
        }
        totalFrames++;
        // Tính FPS trung bình mỗi 0.5 giây
        frameCount++;
        float elapsedSinceFPSUpdate = std::chrono::duration<float>(now - fpsTimestamp).count();
        if (elapsedSinceFPSUpdate >= 0.5f) {
            currentFPS = frameCount / elapsedSinceFPSUpdate;
            frameCount = 0;
            fpsTimestamp = now;
        }

    }

    float getDeltaTime() const { return deltaTime; }
    float getFPS() const { return currentFPS; }
    
    float updateAndGetFPS() {
        auto now = std::chrono::high_resolution_clock::now();
        
        // Tính thời gian trôi qua kể từ lần gọi hàm này trước đó
        static auto lastCall = now; 
        std::chrono::duration<float> elapsed = now - lastCall;
        lastCall = now;

        float dt = elapsed.count();
        
        // Tránh chia cho 0 nếu máy quá nhanh
        if (dt > 0.0f) {
            return 1.0f / dt;
        }
        return 0.0f;
    }

    // Phiên bản mượt hơn (EMA - Exponential Moving Average)
    float getInstantFPS() {
        auto now = std::chrono::high_resolution_clock::now();
        static auto lastFrame = now;
        static float smoothedFPS = 0.0f;

        float frameTime = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame = now;

        if (frameTime > 0) {
            float current = 1.0f / frameTime;
            // Công thức LERP để số nhảy không quá gắt
            smoothedFPS = smoothedFPS * 0.9f + current * 0.1f;
        }
        return smoothedFPS;
    }
    bool hasReached(uint64_t target) const {
        return totalFrames >= target;
    }
    bool isEvery() const {
        if (every_frame <= 0 || totalFrames == 0) return false;
        double result = std::fmod((double)totalFrames, (double)every_frame);
        return (result < 1.0);
    }
    void set_ev_frame(double target_n, float delay_seconds = 0.5f) {
        // Trường hợp 1: Chuyển từ CHẬM sang NHANH (Số frame bỏ qua giảm xuống)
        // Ví dụ: every_frame đang là 30, target_n là 1
        if (target_n < every_frame) {
            every_frame = target_n;         // Áp dụng ngay lập tức
            pending_every_frame = target_n; // Đồng bộ biến chờ
            transition_timer = 0.0f;        // Hủy bỏ mọi bộ đếm đợi
        } 
        // Trường hợp 2: Chuyển từ NHANH sang CHẬM (Số frame bỏ qua tăng lên)
        // Ví dụ: every_frame đang là 1, target_n là 15 hoặc 30
        else if (target_n > every_frame) {
            if (target_n != pending_every_frame) {
                pending_every_frame = target_n; // Đặt mục tiêu mới vào hàng chờ
                transition_timer = delay_seconds; // Bắt đầu đếm ngược delay
            }
        }
        // Nếu target_n == every_frame thì không làm gì cả
    }
    void resetTotalFrames() {
        totalFrames = 0;
    }
    uint64_t getTotalFrames() const { return totalFrames; }

private:
    int targetFPS;
    std::chrono::microseconds frameDelay;
    std::chrono::high_resolution_clock::time_point startPoint;
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    
    // Các biến phục vụ tính FPS
    std::chrono::high_resolution_clock::time_point fpsTimestamp;
    int frameCount = 0;
    float currentFPS = 0.0f;
    float deltaTime = 0.0f;
    
    uint64_t totalFrames = 0;

    double every_frame = 1;
    double pending_every_frame = 1; // Giá trị mới đang đợi áp dụng
    float transition_timer = 0.0f;  // Bộ đếm thời gian
    float transition_delay = 0.5f; // Độ trễ mặc định là 0.5s
};

#endif
#endif
