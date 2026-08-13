#pragma once

#include "../core/gui_types.h"

struct PlayPauseOverlay {
    float alpha = 0.0f;
    float scale = 1.0f;
    bool last_paused = false; 
    bool initialized = false;
};

struct SeekingData {
    float timer = 0.0f;
    float alpha = 0.0f;   
    float pulse = 0.0f;   
    float lastTime = 0.0f;
    float currentTime = 0.0f;

    ImVec2 pos = ImVec2(0, 0);  
    ImVec2 size = ImVec2(0, 0);

    bool forward = true;
    bool lastSeekingState = false;
    bool g_isSeeking =false;
};

namespace CSImGui {
    void DrawCardWithHole(ImDrawList *dl, const ImVec2 &cardMin,
                          const ImVec2 &cardMax, const ImVec2 &holeMin, const ImVec2 &holeMax,
                          ImU32 fillCol, ImU32 borderCol, const CardHoleStyle &style);
}