// WindowRuntime.h (Cập nhật)
#pragma once
#include <SDL.h>
#include <windows.h>
#include <memory>
#include "WindowDefs.h"
#include "WindowPropertyBag.h"
#include "WindowRenderer.h"
#include "WindowController.h"

#include "MPVSession.h"
#include "IGraphicsBackend.h" // Thêm include

#include "utils.h" 
// Forward declaration
class MPVSession;
class IGraphicsBackend;
class WindowRenderer;
class WindowController;
class FrameTimer;

using WindowId = uint32_t;
/**
 * @brief 
 * 
 */
class WindowRuntime {
public:
    WindowId id = 0;
    SDL_Window* sdlWindow = nullptr;
    HWND hwnd = nullptr;

    ImGuiContext* imguiCtx = nullptr;
    std::unique_ptr<IGraphicsBackend> graphicsBackend ; // Thay thế cho SDL_GLContext[cite: 14]

    WindowState state;
    WindowStyle style;
    PropertyBag properties;
    
    std::unique_ptr<WindowRenderer> renderer;
    std::unique_ptr<WindowController> controller;
    
    MPVSession* mpvSession = nullptr; // Chỉ giữ con trỏ, không sở hữu
    // Mỗi cửa sổ có một FrameTimer riêng để quản lý tần suất cập nhật của nó.
    std::unique_ptr<FrameTimer> windowloop;

    WindowRuntime(WindowId id = 0, SDL_Window* sdlWindow = nullptr, HWND hwnd = nullptr);
    ~WindowRuntime();
};