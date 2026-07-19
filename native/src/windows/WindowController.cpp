// WindowController.cpp
#include "WindowController.h"
#include "WindowRuntime.h"
#include <SDL_syswm.h>

void WindowController::Move(int x, int y) {
    if (!runtime->hwnd) return;
    SetWindowPos(runtime->hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    runtime->state.x = x;
    runtime->state.y = y;
}

void WindowController::Resize(int w, int h) {
    if (!runtime->hwnd) return;
    SetWindowPos(runtime->hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    runtime->state.width = w;
    runtime->state.height = h;
}

void WindowController::ToggleFullscreen() {
    if (!runtime->hwnd || !runtime->sdlWindow) return;
    
    if (!runtime->state.isFullscreen) {
        // 1. LƯU TRẠNG THÁI TRƯỚC KHI FULLSCREEN
        GetWindowRect(runtime->hwnd, &runtime->state.fullscreenRestoreRect);
        
        // Lưu lại trạng thái placement (để biết trước đó có đang Maximize hay không)
        runtime->state.placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(runtime->hwnd, &runtime->state.placement);
        
        SDL_GetWindowSize(runtime->sdlWindow, &runtime->state.restoreW, &runtime->state.restoreH);

        RECT rcScreen;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0);
        SetWindowPos(runtime->hwnd, HWND_TOP, rcScreen.left, rcScreen.top,
                    rcScreen.right - rcScreen.left,
                    rcScreen.bottom - rcScreen.top,
                    SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);


        runtime->state.isFullscreen = true;
    } 
    else {
        // Restore vị trí và kích thước windowed
        RECT rc = runtime->state.fullscreenRestoreRect;
        SetWindowPos(runtime->hwnd, HWND_TOP,
                    rc.left, rc.top,
                    rc.right - rc.left,
                    rc.bottom - rc.top,
                    SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        // Restore trạng thái maximize/minimize
        SetWindowPlacement(runtime->hwnd, &runtime->state.placement);

        // Đồng bộ lại kích thước SDL
        SDL_SetWindowPosition(runtime->sdlWindow, rc.left, rc.top);
        SDL_SetWindowSize(runtime->sdlWindow, runtime->state.restoreW, runtime->state.restoreH);

        runtime->state.isFullscreen = false;
    }
}

void WindowController::Close() {
    if (runtime->sdlWindow) {
        SDL_Event ev;
        ev.type = SDL_WINDOWEVENT;
        ev.window.event = SDL_WINDOWEVENT_CLOSE;
        ev.window.windowID = SDL_GetWindowID(runtime->sdlWindow);
        SDL_PushEvent(&ev);
    }
}

// Định nghĩa Constructor/Destructor cho WindowRuntime tại đây để tránh vòng lặp include
WindowRuntime::WindowRuntime() {
    controller = std::make_unique<WindowController>(this);
}
WindowRuntime::~WindowRuntime() {

    if (graphicsBackend) {
        graphicsBackend->Shutdown(); // Giải phóng API Đồ họa trước[cite: 14]
    }
    if (imguiCtx) {
        ImGui::DestroyContext(imguiCtx);
    }
    if (sdlWindow) {
        SDL_DestroyWindow(sdlWindow);
    }
}