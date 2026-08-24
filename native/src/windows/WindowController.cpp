// WindowController.cpp
#include "WindowController.h"
#include "WindowRuntime.h"
#include <SDL_syswm.h>
#include "player/session/PlayerSession.h"
#include "WindowManager.h" 
#include "WindowRelation.h"
#include "UIRenderThread.h"
#include "UpdateWindowState.h"
#include <algorithm>

void WindowController::Move(int x, int y) {
    if (!runtime->resource.hwnd) return;
    SetWindowPos(runtime->resource.hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void WindowController::Resize(int w, int h) {
    if (!runtime->resource.hwnd) return;
    SetWindowPos(runtime->resource.hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void WindowController::ToggleFullscreen() {
    if (!runtime->resource.hwnd || !runtime->resource.sdlWindow) return;
    
    HWND hwnd = runtime->resource.hwnd;
    bool isFullscreen = false;

    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        isFullscreen = runtime->state.display.isFullscreen;
    }

    if (!isFullscreen) {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(WINDOWPLACEMENT);

        if (!GetWindowPlacement(hwnd, &placement)) return;

        RECT restoreRect{};
        if (!GetWindowRect(hwnd, &restoreRect)) return;

        int restoreW = restoreRect.right - restoreRect.left;
        int restoreH = restoreRect.bottom - restoreRect.top;
        
        {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.geometry.fullscreenRestoreRect = restoreRect;
            runtime->state.geometry.placement = placement;
            runtime->state.geometry.restoreW = restoreW;
            runtime->state.geometry.restoreH = restoreH;
        }

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(MONITORINFO);

        if (!GetMonitorInfo(hwnd ? monitor : nullptr, &mi)) return;

        const RECT& rc = mi.rcWork;

        SetWindowPos(hwnd, HWND_TOP, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.display.isFullscreen = true;
        }
    } 
    else {
        WINDOWPLACEMENT placement{};
        {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            placement = runtime->state.geometry.placement;
        }

        placement.length = sizeof(WINDOWPLACEMENT);
        placement.showCmd = SW_SHOWNORMAL;
        SetWindowPlacement(hwnd, &placement);

        {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.display.isFullscreen = false;
        }
    }
    UpdateWindowState(runtime);
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
    }
}

void WindowController::Minimize() {
    if (runtime->resource.sdlWindow) {
        SDL_MinimizeWindow(runtime->resource.sdlWindow);
    }
}

void WindowController::Restore() {
    if (runtime->resource.sdlWindow) {
        SDL_RestoreWindow(runtime->resource.sdlWindow);
    }
}

void WindowController::SetOpacity(float opacity) {
    if (runtime->resource.sdlWindow) {
        float clamped_opacity = std::max(0.0f, std::min(1.0f, opacity));
        SDL_SetWindowOpacity(runtime->resource.sdlWindow, clamped_opacity);
    }
}

// =========================================================================
// IMPLEMENTATION CÁC HÀM NÂNG CẤP MỚI
// =========================================================================

void WindowController::SetAlwaysOnTop(bool enable) {
    HWND hwnd = runtime->resource.hwnd;
    if (!hwnd) return;

    // Cập nhật thuộc tính Always On Top qua API Win32
    HWND hWndInsertAfter = enable ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(hwnd, hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.display.isPinned = enable;
    }

    UpdateWindowState(runtime);
}

void WindowController::ToggleAlwaysOnTop() {
    bool currentPinned = false;
    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        currentPinned = runtime->state.display.isPinned;
    }
    SetAlwaysOnTop(!currentPinned);
}

void WindowController::Focus() {
    if (runtime->resource.sdlWindow) {
        SDL_RaiseWindow(runtime->resource.sdlWindow);
    }
    if (runtime->resource.hwnd) {
        SetForegroundWindow(runtime->resource.hwnd);
        SetFocus(runtime->resource.hwnd);
    }
}

void WindowController::Show() {
    if (runtime->resource.sdlWindow) {
        SDL_ShowWindow(runtime->resource.sdlWindow);
    }
    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.display.isVisible = true;
    }
}

void WindowController::Hide() {
    if (runtime->resource.sdlWindow) {
        SDL_HideWindow(runtime->resource.sdlWindow);
    }
    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.display.isVisible = false;
    }
}

void WindowController::CenterOnScreen() {
    HWND hwnd = runtime->resource.hwnd;
    if (!hwnd) return;

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (!GetMonitorInfo(hMonitor, &mi)) return;

    RECT winRect;
    if (!GetWindowRect(hwnd, &winRect)) return;

    int winWidth  = winRect.right - winRect.left;
    int winHeight = winRect.bottom - winRect.top;

    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - winWidth) / 2;
    int y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - winHeight) / 2;

    Move(x, y);
}

void WindowController::SetBordered(bool bordered) {
    if (runtime->resource.sdlWindow) {
        SDL_SetWindowBordered(runtime->resource.sdlWindow, bordered ? SDL_TRUE : SDL_FALSE);
    }
}

void WindowController::Flash(bool start) {
    HWND hwnd = runtime->resource.hwnd;
    if (!hwnd) return;

    FLASHWINFO fw = { sizeof(FLASHWINFO) };
    fw.hwnd = hwnd;
    fw.dwFlags = start ? (FLASHW_ALL | FLASHW_TIMERNOFG) : FLASHW_STOP;
    fw.uCount = start ? 5 : 0;
    fw.dwTimeout = 0;
    
    FlashWindowEx(&fw);
}

// =========================================================================
// CONSTRUCTOR & DESTRUCTOR CỦA WINDOWRUNTIME
// =========================================================================

WindowRuntime::WindowRuntime(WindowId _id, SDL_Window* _sdlWindow, HWND _hwnd) {
    info.id = _id;
    resource.sdlWindow = _sdlWindow;
    resource.hwnd = _hwnd;

    controller = std::make_unique<WindowController>(this);
}

WindowRuntime::~WindowRuntime() {
    // 1. Dừng luồng Render trước

    if (resource.GetPlayerSession() && resource.GetPlayerSession()->GetRenderer()) {
        resource.GetPlayerSession()->GetRenderer()->Shutdown();
    }

    if (resource.uiRenderThread) {
        resource.uiRenderThread->Stop();
    }

    // 2. Shutdown Backend
    if (resource.graphicsBackend) {
        resource.graphicsBackend->Shutdown(true);
    }
    // 3. Set Current Context và hủy Context ImGui TRƯỚC khi xóa Font Controller
    if (resource.imguiCtx) {
        ImGui::SetCurrentContext(resource.imguiCtx);
        ImGui::DestroyContext(resource.imguiCtx);
        resource.imguiCtx = nullptr;
    }

    // 4. Xóa FontController (Xóa Atlas) SAU khi Destroy Context
    if (fontController) {
        fontController.reset();
    }

    // 5. Hủy SDL Window vật lý
    if (resource.sdlWindow) {
        SDL_DestroyWindow(resource.sdlWindow);
        resource.sdlWindow = nullptr;
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