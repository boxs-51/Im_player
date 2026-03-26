#pragma once

#include "globals.h"
#include "utils.h"
#include "imgui.h"

#include <SDL.h>
#include "imgui.h"

// Vẽ title bar tùy chỉnh cho SDL borderless window
// Chỉ vùng title bar nhận input, nội dung bên dưới ignore input


void RenderBorderlessWindow(SDL_Window* sdlWindow, const char* title, BorderlessWindowState& state);
void SyncWinAPI_SDL(SDL_Window* window, BorderlessWindowState& state);
void SetWindowSDL(SDL_Window* window,
                  int minW = 0, int minH = 0,
                  int maxW = 0, int maxH = 0,
                  int aspectNum = 0, int aspectDen = 0,
                  bool enableSnap = true, int snapThreshold = 24);



void SDLX_InitBorderless(SDL_Window* window, int titleHeight, int resizeMargin);



