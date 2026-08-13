#pragma once

#include "../../core/gui_types.h"

struct PlayPauseData {
    float t = 0.0f;
    bool paused = false;
    bool hovered = false;
};

void DrawPlayPauseIcon(ImDrawList* dl, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data);
void DrawPlayIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawPauseIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawPrevIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);
void DrawNextIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color);