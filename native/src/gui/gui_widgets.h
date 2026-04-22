#pragma once
#include <utils.h>

#include <mpv/mpv_settings.h>
#include <gui/gui.h>
#include <imgui_internal.h>
#include <imgui.h>
#include <string>
#include <stdarg.h>
#include <map>
#include <regex>
#include <functional>

struct PlayPauseData
{
    float t = 0.0f;      // animation 0→1
    bool paused = false; // state thực của player

    bool hovered = false; // trạng thái hover hiện tại
};
struct VolumeIconData
{
    int   volume = 100;
    bool  isMuted = false;

    float waveT   = 1.0f; // 0..1 : độ hiện sóng
    float muteT   = 0.0f; // 0..1 : độ hiện dấu X
};
struct SettingsIconData {
    bool hovered;
    bool opened;

    // internal animation state
    float hover_t = 0.0f;
    float open_t  = 0.0f;
    float angle   = 0.0f;
};
struct FullscreenIconData
{
    bool fullscreen = false;
    float t = 0.0f; // 0..1 animation
};
struct OptionIconData {
    bool hovered;  
    bool opened;   


    // internal animation state (hàm tự xử)
    float hover_t = 0.0f;
    float open_t  = 0.0f;
};
struct LoadingIconData {

    ImDrawList* drawList;

    float angle = 0.0f;
    float speed = 4.0f;
    
    // Thêm các trường này
    ImVec2 pos = ImVec2(0, 0);  // Tọa độ góc trên bên trái (Screen Space)
    ImVec2 size = ImVec2(0, 0); // Kích thước vùng vẽ
};
struct SeekingIconData {
    float timer = 0.0f;
    float alpha = 0.0f;   
    float pulse = 0.0f;   
    bool  forward = true;

    ImVec2 pos = ImVec2(0, 0);  
    ImVec2 size = ImVec2(0, 0);
    
    bool g_isSeeking =false;
};
struct PlayPauseOverlay {
    float alpha = 0.0f;
    float scale = 1.0f;
    bool last_paused = false; 
    bool initialized = false;
};



void DrawPlayPauseIcon( ImDrawList* dl,ImVec2 pMin,ImVec2 pMax,ImU32 color,void* user_data);
void DrawPlayIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawPauseIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawPrevIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawNextIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawVolumeIcon(ImDrawList* drawList,ImVec2 pMin,ImVec2 pMax,ImU32 color,void* user_data);
void DrawSettingsIconAnimated(ImDrawList* drawList,ImVec2 pMin,ImVec2 pMax,ImU32 color,void* user_data);
void DrawFullscreenIconAnimated(ImDrawList* drawList,ImVec2 pMin,ImVec2 pMax,ImU32 color,void* user_data);
void DrawFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawUnFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawOptionIconAnimated(ImDrawList* drawList,ImVec2 pMin,ImVec2 pMax,ImU32 color,void* user_data);
void DrawLoadingIconAnimated(ImDrawList* drawList,ImVec2 pMin, ImVec2 pMax,ImU32 color,void* user_data);
void DrawSeekingIconAnimated(ImDrawList* drawList,ImVec2 pMin,ImVec2 pMax,ImU32 color,void* user_data);
void DrawGhostStatusOverlay(ImVec2 vPos, ImVec2 vSize, bool isPaused);

std::string safeFormatArg(const char* fmtSpec, va_list args, char type);

void IconWrapperAdapter(ImDrawList* dl,ImVec2 min,ImVec2 max,ImU32 col,void* user_data);

int StringResizeCallback(ImGuiInputTextCallbackData* data);
void RenderModernInputEffect(ImGuiID id, float* pFocusAnim);
bool DrawComboPopupBody(
        const char* label,
        std::string& current_value, 
        const std::vector<std::string>& display_options, 
        float custom_width = 200.0f, 
        int max_items_visible = 5,
        std::function<bool(std::string&)> on_validate_confirm = nullptr);



