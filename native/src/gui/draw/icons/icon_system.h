#pragma once

#include "../../core/gui_types.h"

struct SettingsIconData {
    bool hovered;
    bool opened;
    float hover_t = 0.0f;
    float open_t = 0.0f;
    float angle = 0.0f;
};

struct FullscreenIconData {
    bool fullscreen = false;
    float t = 0.0f;
};

struct OptionIconData {
    bool hovered;  
    bool opened;   
    float hover_t = 0.0f;
    float open_t = 0.0f;
};

struct LoadingIconData {
    ImDrawList* drawList;
    float angle = 0.0f;
    float speed = 4.0f;
    ImVec2 pos = ImVec2(0, 0);
    ImVec2 size = ImVec2(0, 0);
};

void DrawSettingsIconAnimated(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data);
void DrawFullscreenIconAnimated(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data);
void DrawFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawUnFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawOptionIconAnimated(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data);
void DrawLoadingIconAnimated(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data);