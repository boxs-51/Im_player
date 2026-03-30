#ifndef UTILS_H
#define UTILS_H

#pragma once

#include "mpv/mpv_settings.h"

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
namespace fs = std::filesystem;
enum class EndFileErrorType {
    None,
    FormatNotSupported,
    URLExpired,
    NetworkError,
    Other
};
enum class PlaybackState {
    Idle,
    Loading,
    Seeking,
    Playing,
    Paused,
    EndOfFile
};
struct CardHoleStyle
{
    float rounding = 6.0f;
    float borderThickness = 1.5f;
};
struct AudioDeviceInfo {
    std::string name;
    std::string description;
};

struct TrackInfo {
    int id = -1;
    std::string type;           // "audio", "video", "sub", "image", ...
    std::string codec;          // "aac", "h264", ...
    std::string codec_desc;     // human readable codec desc
    std::string codec_profile;  // "LC", "High", ...
    std::string decoder;        // decoder name (e.g. "aac", "h264")
    std::string decoder_desc;   // decoder description
    std::string format_name;    // "fltp", "yuv420p", ...
    int ff_index = -1;
    int main_selection = 0;

    // video-specific
    int demux_w = 0;
    int demux_h = 0;
    double demux_fps = 0.0;

    // audio-specific
    int demux_samplerate = 0;
    int demux_channel_count = 0;
    std::string demux_channels; // "stereo", "5.1", ...

    // common metadata
    std::string language;       // "eng", "vie", ...
    std::string title;
    bool image = false;
    bool albumart = false;
    bool is_default = false;
    bool forced = false;
    bool dependent = false;
    bool visual_impaired = false;
    bool hearing_impaired = false;
    bool external = false;
    bool selected = false;
};
struct PlaylistEntry {
    std::string filename;
    std::string title;

    int id = -1;
    
    bool current = false;
    bool playing = false;
};
struct SubInFo {

    double sub_scale =0.0;
    double sub_Delay = 0.0;

    bool sub_Visible = false;

    std::string sub_codec;
    std::string sub_ass_override;
};
struct MPVPlaybackStatus {
    int volume = 100;
    int stream_pos = 0;
    int cache_buffering_state = 0;
    int g_PlayingIndex = -1;
    int g_PlayingIndex_1 = 0;
    int g_playlist_count = -1;

    double percent_pos =0.0;
    double duration = 0.0;
    double timePos = 0.0;
    double playbackTime = 0.0;
    double speed = 1.0;
    double demuxer_bitrate = 0.0;
    double demuxer_cache_duration = 0.0;
    double demuxer_cache_time = 0.0;
    double audio_buffer = 0.0;
    double time_remaining = 0.0;

    bool idle_active = false;
    bool hasTime = false;    
    bool isPaused = false;
    bool isCoreIdle = false;
    bool eofReached = false;
    bool isMuted = false;
    bool seekable = false;
    bool ilde =false;
    bool demuxer_via_network = false;
    bool hasFile = false;
    bool seeking = false;

    const char* working_directory;
    const char* stream_path;
    const char* filename;
    const char* streamUrl;
    const char* mediaTitle;
    const char* Title;
    const char* fileFormat;
    const char* loopMode;
    const char* audio_client_name ;
    
    SubInFo g_subinfo;
    std::vector<PlaylistEntry> g_playlist;
    std::vector<AudioDeviceInfo> g_audioDevices;
};
struct ChapterInfo {
    double time;     // giây
    std::string title;
};

struct VideoParams{
    // ==== Video ====
    std::string vpixfmt;         
    std::string vprimaries;      
    std::string vgamma;          
    std::string vcolormatrix;    
    std::string vcolorlevels;    
    std::string vstereo_in;
    std::string vchroma_location;
    std::string vaspect_name;
    std::string vlight;
    std::string vsar_name;

    int average_bpp = 0;
    int vrotate = 0;           
    int vwidth = 0;            
    int vheight = 0;            
    int vdisp_w = 0;            
    int vdisp_h = 0;      
    int vcrop_x = 0;
    int vcrop_y = 0;
    int vcrop_w = 0;
    int vcrop_h = 0;

    double vaspect = 0.0;       
    double vpar = 0.0;           
    double vsar = 0.0; 
    double vsig_peak = 0.0; 
};
struct AudioParams{
    // ==== Audio ====
    std::string aformat;        // audio format (vd: "s16", "f32")
    std::string ahr_channels;   // layout kênh chi tiết (vd: "FL FR FC LFE BL BR")
    std::string achannels_str;

    int channel_count = 0;          // số kênh
    int asamplerate = 0;        // sample rate (Hz)
};
struct VideoInfo {
    // ==== Video ====
    int width = 0;              // chiều rộng gốc
    int height = 0;             // chiều cao gốc
    int vbitrate = 0;           // video bitrate (bps)
    int rotate = 0;             // góc xoay (0, 90, 180, 270)    
    double aspect = 0.0;        // aspect ratio
    double estimated_vf_fps_mpv = 0.0; // fps ước lượng từ filter graph   
    double currentFPS = 0.0;    // fps thực tế (rendered, lấy từ mpv stats)
    double minFPS = 0.0;        // fps thấp nhất đo được
    double maxFPS = 0.0;        // fps cao nhất đo được    
    const char* video_format;  // video format/container (vd: "mp4", "mkv")    
    const char* vcodec;         // codec video (vd: "h264")
    std::string description;   // mô tả chung (vd: "1920x1080 [SAR 1:1 DAR 16:9] fps 23.976")
    const char* v_out;

    // ==== Audio ====
    const char* acodec;         // codec audio (vd: "aac")
    double audio_delay = 0.0;
    int achannels = 0;          // số kênh
    
    int asamplerate = 0;        // sample rate (Hz)
    int abitrate = 0;           // audio bitrate (bps)

    int g_current_chapter;

    // ==== Hardware/Device ====
    const char* a_out;
    const char* a_filter;
    const char* audio_device;   // thiết bị audio đang xuất (vd: "wasapi/{device-id}")
    const char* hwdec;          // hardware decoder (vd: "d3d11va", "vaapi", "cuda", "vdpau")

    std::vector<TrackInfo>g_tracks;
    std::vector<ChapterInfo> g_chapters;
    VideoParams g_videoparams;
    AudioParams g_audioarams;
    std::unordered_map<std::string, std::string> metadata; 
    
};       
struct WindowContext {
    SDL_Window* mainWindow = nullptr;
    SDL_GLContext mainGLContext = nullptr;
    ImGuiContext* mainImGuiCtx = nullptr;

    SDL_Window* sidebar_Window = nullptr;
    SDL_GLContext sidebar_GLContext = nullptr;
    ImGuiContext* sidebar_ImGuiCtx = nullptr;
};

struct BorderlessWindowState {
    ImVec2 videoOffset = ImVec2(0,0);
    bool isFullscreen_video = false;
    
    #ifdef CUSTOM_TITLEBAR
        float titleHeight = 25.0f;
    #else
        float titleHeight = 0.0f;
    #endif
    int resizeMargin = 5; // vùng nhạy resize
    int minWidth = 100 ;
    int minHeight = 100 ;
    int WinHeight = 600 ;
    int WinWidth = 800 ;

    RECT  fullscreenRestoreRect_temp;

    float  controlWidth = 0.0f;

};

struct WindowLayout {

    int WinW;
    int WinH;
    int WinX;
    int WinY;

    SDL_Rect titleBar;   // Vùng titlebar
    SDL_Rect videoArea;  // Vùng video/content

    ImVec2 DisplaySize;

    ImVec2 VideoPos;
    ImVec2 VideoSize;

};

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

inline ImVec2 operator*(const ImVec2& lhs, float scalar) {
    return ImVec2(lhs.x * scalar, lhs.y * scalar);
}

inline ImVec2 operator/(const ImVec2& lhs, float scalar) {
    return ImVec2(lhs.x / scalar, lhs.y / scalar);
}
extern WindowLayout Windowlayout;
extern WindowContext ctx;
extern MPVPlaybackStatus g_playbackStatus;
extern VideoInfo g_videoInfo;
extern BorderlessWindowState BW;

void UpdateHoverAnim(float& animValue, bool isHovering, float speed = 12.0f);

void ApplyDynamicMPVConfig(mpv_handle* mpv);
void ApplyStaticMPVConfig(mpv_handle* mpv);
void LoadAllScripts(mpv_handle* mpv);
void UpdateGlobalWindowLayout(SDL_Window* sdlWindow, const BorderlessWindowState& state ,WindowLayout& w);
void InitMPVObservers(mpv_handle* mpv);
void InitPlaybackStatus(mpv_handle* mpv);
void ProcessMPVEvents(mpv_handle* mpv );
void TerminateHandler();
void SignalHandler(int signal);
void UpdateUIState(bool& show_ui_video);
void NotifyActivity(bool& show_ui_video);
bool SetDelayHover( bool isHovering, double delaySeconds = 3.0, const char * id = nullptr ) ;

void DrawCardWithHole(
    ImDrawList* dl,
    const ImVec2& cardMin,
    const ImVec2& cardMax,
    const ImVec2& holeMin,
    const ImVec2& holeMax,
    ImU32 fillCol,
    ImU32 borderCol,
    const CardHoleStyle& style = {}
);

const mpv_node* mpv_node_dict_find(const mpv_node* dict, const char* key);
const char* PlaybackStateToString(PlaybackState state);

PlaybackState GetPlaybackState();

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
    const char* tmp=nullptr;
    if(!mpv_get_property(mpv,name.c_str(),MPV_FORMAT_STRING,&tmp)) return false;
    out = tmp ? tmp : "";
    return true;
}

inline ImVec4 sdl_rec_to_imvec4(SDL_Rect &r) {return ImVec4((float) r.x, (float)r.y, (float)(r.x + r.w), (float)(r.y + r.h));}
inline ImVec4 sdl_rec_to_imvec4_raw(SDL_Rect &r) {return ImVec4((float)r.x, (float)r.y, (float)r.w, (float)r.h);}
inline ImVec2 sdl_rec_to_imvec2_pos(SDL_Rect &r) {return ImVec2((float)r.x, (float)r.y);}
inline ImVec2 sdl_rec_to_imvec2_size(SDL_Rect &r) {return ImVec2((float)r.w, (float)r.h);}


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
#endif
