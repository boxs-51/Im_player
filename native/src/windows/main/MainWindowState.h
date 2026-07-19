#include <SDL.h>
#include "imgui.h"
#include "WindowRuntime.h"

struct MainWindowLayout {
    int WinX = 0;
    int WinY = 0;
    int WinW = 800;
    int WinH = 600;

    ImVec2 VideoPos{0, 0};
    ImVec2 VideoSize{800, 600};

    ImVec2 TitlePos{0, 0};
    ImVec2 TitleSize{800, 30};

    SDL_Rect videoArea{0, 0, 800, 600};
};
void UpdateWindowStateCommon(WindowRuntime* runtime);
void UpdateMainWindowState(WindowRuntime* runtime);