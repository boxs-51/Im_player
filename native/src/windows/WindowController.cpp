// WindowController.cpp
#include "WindowController.h"
#include "WindowRuntime.h"
#include <SDL_syswm.h>
#include "player/session/PlayerSession.h"
#include "WindowManager.h" 
#include "WindowRelation.h"
#include "UIRenderThread.h"

void WindowController::Move(int x, int y) {
    if (!runtime->resource.hwnd) return;
    SetWindowPos(runtime->resource.hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    runtime->state.geometry.x = x;
    runtime->state.geometry.y = y;
}

void WindowController::Resize(int w, int h) {
    if (!runtime->resource.hwnd) return;
    SetWindowPos(runtime->resource.hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    runtime->state.geometry.width = w;
    runtime->state.geometry.minWidth = h;
}

void WindowController::ToggleFullscreen() {
    if (!runtime->resource.hwnd || !runtime->resource.sdlWindow) return;
    
    if (!runtime->state.display.isFullscreen) {
        // 1. LƯU TRẠNG THÁI TRƯỚC KHI FULLSCREEN
        GetWindowRect(runtime->resource.hwnd, &runtime->state.geometry.fullscreenRestoreRect);
        
        // Lưu lại trạng thái placement (để biết trước đó có đang Maximize hay không)
        runtime->state.geometry.placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(runtime->resource.hwnd, &runtime->state.geometry.placement);
        
        SDL_GetWindowSize(runtime->resource.sdlWindow, &runtime->state.geometry.restoreW, &runtime->state.geometry.restoreH);

        RECT rcScreen;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0);
        SetWindowPos(runtime->resource.hwnd, HWND_TOP, rcScreen.left, rcScreen.top,
                    rcScreen.right - rcScreen.left,
                    rcScreen.bottom - rcScreen.top,
                    SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);


        runtime->state.display.isFullscreen = true;
    } 
    else {
        // Restore vị trí và kích thước windowed
        RECT rc = runtime->state.geometry.fullscreenRestoreRect;
        SetWindowPos(runtime->resource.hwnd, HWND_TOP,
                    rc.left, rc.top,
                    rc.right - rc.left,
                    rc.bottom - rc.top,
                    SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        // Restore trạng thái maximize/minimize
        SetWindowPlacement(runtime->resource.hwnd, &runtime->state.geometry.placement);

        // Đồng bộ lại kích thước SDL
        SDL_SetWindowPosition(runtime->resource.sdlWindow, rc.left, rc.top);
        SDL_SetWindowSize(runtime->resource.sdlWindow, runtime->state.geometry.restoreW, runtime->state.geometry.restoreH);

        runtime->state.display.isFullscreen = false;
    }
}

void WindowController::Close() {
    if (runtime->resource.sdlWindow) {
        SDL_Event ev;
        ev.type = SDL_WINDOWEVENT;
        ev.window.event = SDL_WINDOWEVENT_CLOSE;
        ev.window.windowID = SDL_GetWindowID(runtime->resource.sdlWindow);
        SDL_PushEvent(&ev);
    }
}

void WindowController::SetTitle(const std::string& title) {
    if (runtime->resource.sdlWindow) {
        SDL_SetWindowTitle(runtime->resource.sdlWindow, title.c_str());
    }
}

void WindowController::Maximize() {
    if (runtime->resource.sdlWindow) {
        SDL_MaximizeWindow(runtime->resource.sdlWindow);
        //runtime->state.isMaximized = true;
    }
}

void WindowController::Minimize() {
    if (runtime->resource.sdlWindow) {
        SDL_MinimizeWindow(runtime->resource.sdlWindow);
        //runtime->state.isMinimized = true;
    }
}

void WindowController::Restore() {
    if (runtime->resource.sdlWindow) {
        SDL_RestoreWindow(runtime->resource.sdlWindow);
        //runtime->state.isMaximized = false;
        //runtime->state.isMinimized = false;
    }
}

void WindowController::SetOpacity(float opacity) {
    if (runtime->resource.sdlWindow) {
        // Đảm bảo giá trị opacity nằm trong khoảng hợp lệ [0.0, 1.0]
        float clamped_opacity = std::max(0.0f, std::min(1.0f, opacity));
        SDL_SetWindowOpacity(runtime->resource.sdlWindow, clamped_opacity);
    }
}

// Định nghĩa Constructor/Destructor cho WindowRuntime tại đây để tránh vòng lặp include
WindowRuntime::WindowRuntime(WindowId _id, SDL_Window* _sdlWindow, HWND _hwnd) {
    info.id = _id;
    resource.sdlWindow = _sdlWindow;
    resource.hwnd = _hwnd;

    controller = std::make_unique<WindowController>(this);
}
WindowRuntime::~WindowRuntime() {
    if(resource.imguiCtx)
        ImGui::SetCurrentContext(resource.imguiCtx);
        
    if (resource.uiRenderThread) {
        resource.uiRenderThread->Stop();
    }
    if (resource.graphicsBackend) {
        // Mỗi luồng tự shutdown backend của nó
        resource.graphicsBackend->Shutdown(true);
    }

    if (resource.imguiCtx) {
        ImGui::DestroyContext(resource.imguiCtx);
        resource.imguiCtx = nullptr;
    }
    if (resource.sdlWindow) {
        SDL_DestroyWindow(resource.sdlWindow);
    }
}

// --- Cài đặt các hàm tiện ích mới của WindowRuntime ---

WindowRuntime* WindowRuntime::GetParent() {
    return relation.parent;
}

std::vector<WindowRuntime*> WindowRuntime::GetChildren() {
    return relation.children;
}

bool WindowRuntime::HasVisibleChildren() {
    // Sử dụng std::function để tạo một hàm đệ quy
    std::function<bool(WindowId)> checkRecursively = 
        [&](WindowId currentId) -> bool {
        WindowRuntime* currentWin = WindowManager::GetInstance().GetWindowById(currentId);
        if (!currentWin) return false;

        for (WindowRuntime* child : currentWin->relation.children) {
            if (child && (child->state.display.isVisible || checkRecursively(child->info.id))) {
                return true;
            }
        }
        return false;
    };

    return checkRecursively(this->info.id);
}