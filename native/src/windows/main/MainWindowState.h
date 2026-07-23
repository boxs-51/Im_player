#pragma once
#include <SDL.h>
#include "imgui.h"
#include "WindowRuntime.h"
// Cấu trúc layout CHUNG cho TẤT CẢ các loại cửa sổ
struct WindowLayout {
    int WinX = 0;
    int WinY = 0;
    int WinW = 800;
    int WinH = 600;

    ImVec2 ClientPos{0, 0};
    ImVec2 ClientSize{800, 600};

    ImVec2 TitlePos{0, 0};
    ImVec2 TitleSize{800, 30};

    SDL_Rect ClientArea{0, 0, 800, 600};
};

void RenderTitleBarWindowObject(WindowRuntime* runtime, const char* title, ImVec2 _winPos, ImVec2 _winSize);
