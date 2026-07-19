// WindowRuntime.h (Cập nhật)
#pragma once
#include <SDL.h>
#include <windows.h>
#include <memory>
#include "WindowDefs.h"
#include "WindowPropertyBag.h"
#include "WindowRenderer.h"
#include "WindowController.h"
#include "IGraphicsBackend.h" // Thêm include

using WindowId = uint32_t;

class WindowRuntime {
public:
    WindowId id = 0;
    SDL_Window* sdlWindow = nullptr;
    HWND hwnd = nullptr;

    ImGuiContext* imguiCtx = nullptr;
    std::unique_ptr<IGraphicsBackend> graphicsBackend = nullptr; // Thay thế cho SDL_GLContext[cite: 14]

    WindowState state;
    WindowStyle style;
    PropertyBag properties;
    
    std::unique_ptr<WindowRenderer> renderer;
    std::unique_ptr<WindowController> controller;

    WindowRuntime();
    ~WindowRuntime();
};