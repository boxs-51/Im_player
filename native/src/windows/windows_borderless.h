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
//TODO: Chuyen logic sang mot file utils.h rieng cho window/SDL
namespace SDLUtils{
void SDLX_InitBorderless(SDL_Window* window);
void SDLX_PushUniqueEvent(const SDL_Event& eventData);
void SDLX_SetMinMax(SDL_Window* window, ImVec2& winmin = ImVec2(0,0), ImVec2& winmax = ImVec2(0,0));
void SyncSDLWithWinAPI(SDL_Window* sdlWin);
void SDLX_PushEvent(SDL_Window* win, Uint8 evt, int d1=0, int d2=0);
void SDLX_PushClose(SDL_Window* /*win*/);
// Fullscreen
bool SDLX_ToggleFullscreen(SDL_Window* window, bool enable);
void SetWindowSDL(SDL_Window* window, ImVec2& winmin = ImVec2(0,0), ImVec2& winmax = ImVec2(0,0));

RECT GetMonitorRectForWindow(HWND hwnd);
};
// UI
void RenderBorderlessWindow(SDL_Window* sdlWindow, const char* title, DragResizeState& state ,ImVec2 _winPos ,ImVec2 _winSize);