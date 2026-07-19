#ifndef UTILS_H
#define UTILS_H

#pragma once


#include <imgui_internal.h>
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
#include <sstream>
namespace fs = std::filesystem;
/*
// --- Kiểu số nguyên không dấu (Unsigned Integers) ---
typedef uint8_t  Uint8;  //  8-bit: 0 đến 255 (2^8 - 1)
typedef uint16_t Uint16; // 16-bit: 0 đến 65,535 (2^16 - 1)
typedef uint32_t Uint32; // 32-bit: 0 đến 4,294,967,295 (2^32 - 1)
typedef uint64_t Uint64; // 64-bit: 0 đến 18,446,744,073,709,551,615 (2^64 - 1)
// --- Kiểu số nguyên có dấu (Signed Integers) ---
typedef int8_t   Int8;   //  8-bit: -128 đến 127
typedef int16_t  Int16;  // 16-bit: -32,768 đến 32,767
typedef int32_t  Int32;  // 32-bit: -2,147,483,648 đến 2,147,483,647
typedef int64_t  Int64;  // 64-bit: -9,223,372,036,854,775,808 đến 9,223,372,036,854,775,807

// --- Kiểu dữ liệu tùy chỉnh cho Project ---
*/
typedef unsigned int Uint;     // Thường là 32-bit (phụ thuộc vào compiler/Hệ điều hành)
typedef uint32_t     UCol_32;  // 32-bit: Chuyên dùng cho mã màu RGBA (0xRRGGBBAA)
typedef int          Int;      // Thường là 32-bit
/*
typedef bool Bool;  
typedef float Float;
typedef double Double;

typedef std::string String;     // Chuỗi ký tự UTF-8
typedef std::wstring WString;   // Chuỗi ký tự rộng (UTF-16 trên Windows)
typedef std::vector<WString> WStringList; // Danh sách chuỗi rộng
typedef std::vector<String> StringList;   // Danh sách chuỗi UTF-8
typedef std::vector<void*> Vector;    // Danh sách con trỏ void (có thể chứa bất kỳ loại con trỏ nào)
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

inline ImU32 ToIUCol32(const ImVec4& c, float s = 255.0f) {return IM_COL32((Uint)(c.x*s), (Uint)(c.y*s), (Uint)(c.z*s), (Uint)(c.w*s));}
inline ImU32 ToIUCol32(const Col32& c, float s = 255.0f) {return IM_COL32((Uint)(c.r*s), (Uint)(c.b*s), (Uint)(c.g*s), (Uint)(c.a*s));}
inline ImU32 ToIUCol32(const Vec4& c, float s = 255.0f) {return IM_COL32((Uint)(c.x*s), (Uint)(c.y*s), (Uint)(c.w*s), (Uint)(c.h*s));}
inline ImU32 ToIUCol32(const UCol_32& c) {return (Uint)c;}
*/
//inline UCol_32 ToCol32 (const Col32& v, float s = 255.0f) {return COL_32((Uint)(v.r*s), (Uint)(v.b*s), (Uint)(v.g*s), (Uint)(v.a*s));}
//inline UCol_32 ToCol32 (const Vec4& v, float s = 255.0f) {return COL_32((Uint)(v.x*s) ,(Uint)(v.y*s) ,(Uint)(v.w*s) ,(Uint)(v.h*s));}
inline UCol_32 ToCol32 (const ImVec4& v, float s = 255.0f) {return IM_COL32((Uint)(v.x*s) ,(Uint)(v.y*s) ,(Uint)(v.z*s) ,(Uint)(v.w*s));}
//inline UCol_32 ToCol32 (const ImU32& v) {return (Uint)v;}
/*
inline ImVec2 ToImVec2 (const Vec2& v) {return ImVec2{v.x, v.y};}
inline ImVec4 ToImVec4 (const Vec4& v) {return ImVec4{v.x, v.y, v.w, v.h};}
inline ImVec4 ToImVec4(ImU32 c)
{
    float r = (float)((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
    float g = (float)((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
    float b = (float)((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
    float a = (float)((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
    return ImVec4(r, g, b, a);
}
inline Vec2 ToVec2 (const ImVec2& v) {return Vec2{v.x, v.y};}
inline Vec4 ToVec4 (const ImVec4& v) {return Vec4{v.x, v.y, v.z, v.w};}
*/

inline std::string Format(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}

inline float ImLength(ImVec2& dhs) { return {dhs.x * dhs.x + dhs.y * dhs.y}; }

inline float ImLerp(float a, float b, float t) { return a + (b - a) * t; }
/*inline ImVec2 ImLerp(const ImVec2& a, const ImVec2& b, float t)
{
    return ImVec2(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    );
}*/
/*inline ImVec4 ImLerp(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}*/
inline ImU32 ImLerp(ImU32 a, ImU32 b, float t) {
    // Giới hạn t trong khoảng [0.0, 1.0]
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;

    // Tách các kênh màu
    ImU32 a_r = (a >> IM_COL32_R_SHIFT) & 0xFF;
    ImU32 a_g = (a >> IM_COL32_G_SHIFT) & 0xFF;
    ImU32 a_b = (a >> IM_COL32_B_SHIFT) & 0xFF;
    ImU32 a_a = (a >> IM_COL32_A_SHIFT) & 0xFF;

    ImU32 b_r = (b >> IM_COL32_R_SHIFT) & 0xFF;
    ImU32 b_g = (b >> IM_COL32_G_SHIFT) & 0xFF;
    ImU32 b_b = (b >> IM_COL32_B_SHIFT) & 0xFF;
    ImU32 b_a = (b >> IM_COL32_A_SHIFT) & 0xFF;

    // Nội suy từng kênh và đóng gói lại
    return IM_COL32(
        (ImU32)(a_r + (b_r - a_r) * t),
        (ImU32)(a_g + (b_g - a_g) * t),
        (ImU32)(a_b + (b_b - a_b) * t),
        (ImU32)(a_a + (b_a - a_a) * t)
    );
}



// --- Đối với ImVec2 ---

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
inline ImVec2 operator/(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x / rhs.x, lhs.y / rhs.y); }
inline ImVec2 operator-(const ImVec2& lhs) { return ImVec2(-lhs.x, -lhs.y); }

inline ImVec2 operator*(const ImVec2& lhs, float scalar) {return ImVec2{lhs.x * scalar, lhs.y * scalar}; } 
inline ImVec2 operator*(float scalar, const ImVec2& lhs) {return ImVec2{lhs.x * scalar, lhs.y * scalar}; }
    
inline ImVec2 operator/(const ImVec2& lhs, float scalar) {return ImVec2{lhs.x / scalar, lhs.y / scalar}; } 
inline ImVec2 operator/(float scalar, const ImVec2& lhs) {return ImVec2{lhs.x / scalar, lhs.y / scalar}; }

// Toán tử gán phải thay đổi lhs và trả về tham chiếu
inline ImVec2& operator+=(ImVec2& lhs, float scalar) { lhs.x += scalar; lhs.y += scalar; return lhs; }
inline ImVec2& operator-=(ImVec2& lhs, float scalar) { lhs.x -= scalar; lhs.y -= scalar; return lhs; }
inline ImVec2& operator*=(ImVec2& lhs, float scalar) { lhs.x *= scalar; lhs.y *= scalar; return lhs; }
inline ImVec2& operator/=(ImVec2& lhs, float scalar) { lhs.x /= scalar; lhs.y /= scalar; return lhs; }

inline ImVec2& operator+=(ImVec2& lhs, const ImVec2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }
inline ImVec2& operator-=(ImVec2& lhs, const ImVec2& rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; return lhs; }
inline ImVec2& operator*=(ImVec2& lhs, const ImVec2& rhs) { lhs.x *= rhs.x; lhs.y *= rhs.y; return lhs; }
inline ImVec2& operator/=(ImVec2& lhs, const ImVec2& rhs) { lhs.x /= rhs.x; lhs.y /= rhs.y; return lhs; }

// --- Đối với Vec2 ---
/*
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
*/
// Toán tử số học cơ bản
inline ImVec4 operator+(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w); }
inline ImVec4 operator-(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w); }

// Toán tử gán (Sửa lỗi logic: thay đổi trực tiếp lhs và trả về tham chiếu)
inline ImVec4& operator+=(ImVec4& lhs, float scalar) { lhs.x += scalar; lhs.y += scalar; lhs.z += scalar; lhs.w += scalar; return lhs; }
inline ImVec4& operator-=(ImVec4& lhs, float scalar) { lhs.x -= scalar; lhs.y -= scalar; lhs.z -= scalar; lhs.w -= scalar; return lhs; }
inline ImVec4& operator*=(ImVec4& lhs, float scalar) { lhs.x *= scalar; lhs.y *= scalar; lhs.z *= scalar; lhs.w *= scalar; return lhs; }
inline ImVec4& operator/=(ImVec4& lhs, float scalar) { lhs.x /= scalar; lhs.y /= scalar; lhs.z /= scalar; lhs.w /= scalar; return lhs; }

// Toán tử số học cơ bản
/*
inline Vec4 operator+(const Vec4& lhs, const Vec4& rhs) { return Vec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.w + rhs.w, lhs.h + rhs.h); }
inline Vec4 operator-(const Vec4& lhs, const Vec4& rhs) { return Vec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.w - rhs.w, lhs.h - rhs.h); }
inline Vec4 operator*(const Vec4& lhs, const Vec4& rhs) { return Vec4(lhs.x * rhs.x, lhs.y * rhs.y, lhs.w * rhs.w, lhs.h * rhs.h); }
inline Vec4 operator/(const Vec4& lhs, const Vec4& rhs) { return Vec4(lhs.x / rhs.x, lhs.y / rhs.y, lhs.w / rhs.w, lhs.h / rhs.h); }

// Toán tử gán
inline Vec4& operator+=(Vec4& lhs, float scalar) { lhs.x += scalar; lhs.y += scalar; lhs.w += scalar; lhs.h += scalar; return lhs; }
inline Vec4& operator-=(Vec4& lhs, float scalar) { lhs.x -= scalar; lhs.y -= scalar; lhs.w -= scalar; lhs.h -= scalar; return lhs; }
inline Vec4& operator*=(Vec4& lhs, float scalar) { lhs.x *= scalar; lhs.y *= scalar; lhs.w *= scalar; lhs.h *= scalar; return lhs; }
inline Vec4& operator/=(Vec4& lhs, float scalar) { lhs.x /= scalar; lhs.y /= scalar; lhs.w /= scalar; lhs.h /= scalar; return lhs; }
*/

void UpdateHoverAnim(float& animValue, bool isHovering, float speed = 12.0f);

void ApplyDynamicMPVConfig(mpv_handle* mpv);
void ApplyStaticMPVConfig(mpv_handle* mpv);
void LoadAllScripts(mpv_handle* mpv);
void TerminateHandler();
void SignalHandler(int signal);
void NotifyActivity(bool& show_ui_video);
bool SetDelayHover(bool hovering, double delaySeconds = 3.0f, ImGuiID id = 0) ;


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
/*
inline Vec4 ToVec4(SDL_Rect &r) {return Vec4((float) r.x, (float)r.y, (float)(r.x + r.w), (float)(r.y + r.h));}
inline Vec4 ToVec4_RAW(SDL_Rect &r) {return Vec4((float)r.x, (float)r.y, (float)r.w, (float)r.h);}
inline Vec2 ToVec2_Pos(SDL_Rect &r) {return Vec2((float)r.x, (float)r.y);}
inline Vec2 ToVec2_Size(SDL_Rect &r) {return Vec2((float)r.w, (float)r.h);}
*/

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
    inline std::string TruncateTextByPixelsLines(
        const char* text,
        float max_width,
        int max_lines,
        ImFont* font,
        float font_size)
    {
        std::string result;
        const char* s = text;
        int lines = 0;

        while (*s && lines < max_lines)
        {
            const char* line_start = s;
            float line_width = 0.0f;
            const char* last_fit = s;

            while (*s && *s != '\n')
            {
                const char* prev = s;
                unsigned int c;
                s += ImTextCharFromUtf8(&c, s, NULL);

                float char_w = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, prev, s).x;

                if (line_width + char_w > max_width)
                    break;

                line_width += char_w;
                last_fit = s;
            }

            result.append(line_start, last_fit);

            if (*s && *s != '\n')
            {
                result += "...";
                break;
            }

            if (*s == '\n')
            {
                result += '\n';
                s++;
            }

            lines++;
        }

        return result;
    }
    inline std::vector<std::string> WrapTextToLines(ImFont* font, float fontSize, const std::string& text, float max_width, int max_lines = 0) {
        std::vector<std::string> lines;
        if (text.empty()) return lines;

        std::stringstream ss(text);
        std::string segment;

        while (std::getline(ss, segment, '\n')) {
            std::stringstream ss_word(segment);
            std::string word;
            std::string current_line = "";

            while (ss_word >> word) {
                // Kiểm tra xem đã đạt giới hạn dòng chưa
                if (max_lines > 0 && lines.size() >= (size_t)max_lines) break;

                std::string test_line = current_line.empty() ? word : current_line + " " + word;
                float line_w = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, test_line.c_str()).x;

                if (line_w <= max_width) {
                    current_line = test_line;
                } else {
                    if (!current_line.empty()) lines.push_back(current_line);
                    
                    // Nếu dòng mới này vượt quá max_lines, dừng lại
                    if (max_lines > 0 && lines.size() >= (size_t)max_lines) {
                        current_line = "";
                        break;
                    }
                    current_line = word;
                }
            }
            
            if (!current_line.empty()) {
                if (max_lines > 0 && lines.size() >= (size_t)max_lines) {
                    // Đã đủ dòng, không thêm nữa
                } else {
                    lines.push_back(current_line);
                }
            }
            
            if (max_lines > 0 && lines.size() >= (size_t)max_lines) break;
        }

        // Xử lý thêm dấu "..." nếu văn bản còn dư
        // (Kiểm tra đơn giản: nếu chuỗi gốc dài hơn tổng các ký tự trong list dòng)
        // Hoặc kiểm tra nếu vòng lặp bị break sớm.
        if (max_lines > 0 && lines.size() == (size_t)max_lines) {
            std::string& last_line = lines.back();
            // Thay thế 3 ký tự cuối bằng "..." hoặc cộng thêm nếu còn đủ chỗ
            float dots_w = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "...").x;
            
            // Thu hẹp dòng cuối lại để nhét vừa dấu "..."
            while (!last_line.empty() && (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, last_line.c_str()).x + dots_w) > max_width) {
                if (!last_line.empty()) last_line.pop_back();
            }
            last_line += "...";
        }

        return lines;
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
