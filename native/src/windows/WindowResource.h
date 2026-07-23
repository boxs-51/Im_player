// WindowResource.h
#pragma once
#include <SDL.h>
#include <windows.h>
#include <memory>
#include "imgui.h"

// Forward declarations
class IGraphicsBackend;
class UIRenderThread;
class MPVSession;
class WindowSharedGroup;

struct WindowResource {
    SDL_Window* sdlWindow = nullptr;
    HWND hwnd = nullptr;
    ImGuiContext* imguiCtx = nullptr;
    std::unique_ptr<IGraphicsBackend> graphicsBackend;
    std::unique_ptr<UIRenderThread> uiRenderThread;
    MPVSession* mpvSession = nullptr;
    std::shared_ptr<WindowSharedGroup> sharedGroup; // Chỉ root window mới sở hữu
};