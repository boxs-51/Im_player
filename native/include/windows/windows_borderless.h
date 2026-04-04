#pragma once

#include "windows/windows_borderless_state.h"
#include "globals.h"
#include "utils.h"
#include "imgui.h"

#include <SDL.h>

#include <windows.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <wrl.h>

// Core

void SDLX_SetMinMax(SDL_Window* window, int minW, int minH, int maxW, int maxH);
void SyncSDLWithWinAPI(SDL_Window* sdlWin);
void SDLX_PushEvent(SDL_Window* win, Uint8 evt, int d1=0, int d2=0);
void SDLX_PushClose(SDL_Window* /*win*/);
// Fullscreen
bool SDLX_ToggleFullscreen(SDL_Window* window, bool enable);
// UI
void RenderBorderlessWindow(SDL_Window* sdlWindow, const char* title, DragResizeState& state ,ImVec2 winPos ,ImVec2 winSize);

void SetWindowSDL(SDL_Window* window,
                  int minW = 0, int minH = 0,
                  int maxW = 0, int maxH = 0);

inline RECT GetMonitorRectForWindow(HWND hwnd) {
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (hMon) GetMonitorInfo(hMon, &mi);
    return mi.rcWork; // rcWork = vùng khả dụng (không tính taskbar)
}

