#pragma once

#include "globals.h"
#include "utils.h"
#include "imgui.h"

#include <SDL.h>
#include "imgui.h"

// Vẽ title bar tùy chỉnh cho SDL borderless window
// Chỉ vùng title bar nhận input, nội dung bên dưới ignore input

void SDLX_InitBorderless(SDL_Window* window, int titleHeight, int resizeMargin);





