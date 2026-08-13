#pragma once

#include "../../core/gui_types.h"

struct VolumeIconData {
    int volume = 100;
    bool isMuted = false;
    float waveT = 1.0f;
    float muteT = 0.0f;
};

void DrawVolumeIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data);