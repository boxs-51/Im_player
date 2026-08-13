// WindowProcHook.cpp
#include "WindowRuntime.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include "UpdateWindowState.h"
#include "WindowResource.h"
#include "WindowManager.h"
#include "UIRenderThread.h"
#include "EventQueue.h"

#include "player/session/PlayerSession.h"

#include <windowsx.h>

// Định nghĩa mã Custom Hit-Test cho Titlebar
#define HTCUSTOM_MIN      1000
#define HTCUSTOM_MAX      1001
#define HTCUSTOM_CLOSE    1002
#define HTCUSTOM_RETORE   1003
#define HTCUSTOM_PIN      1004 

#define _WIN32_WINNT 0x0A00
#define IDT_RENDER_TIMER  1005

// Detect góc cạnh để resize
static ResizeEdge MultiWindowDetectEdge(WindowRuntime* runtime, int localX, int localY, int winW, int winH) {
    if (runtime->state.display.isMaximized || runtime->state.display.isFullscreen) 
        return ResizeEdge::NONE;

    int margin = runtime->style.resizeMargin;
    const bool left   = (localX <= margin);
    const bool right  = (localX >= winW - margin);
    const bool top    = (localY <= margin);
    const bool bottom = (localY >= winH - margin);

    if (top && left)     return ResizeEdge::TOPLEFT;
    if (top && right)    return ResizeEdge::TOPRIGHT;
    if (bottom && left)  return ResizeEdge::BOTTOMLEFT;
    if (bottom && right) return ResizeEdge::BOTTOMRIGHT;
    if (left)   return ResizeEdge::LEFT;
    if (right)  return ResizeEdge::RIGHT;
    if (top)    return ResizeEdge::TOP;
    if (bottom) return ResizeEdge::BOTTOM;
    return ResizeEdge::NONE;
}

enum class WindowScreenState {
    Normal,         // Cửa sổ dạng Windowed bình thường
    Maximized,      // Cửa sổ Phóng to (vẫn hiện Taskbar)
    Fullscreen      // Cửa sổ Toàn màn hình (đè lên cả Taskbar)
};

/**
 * @brief Kiểm tra trạng thái hiển thị màn hình thực tế từ Windows HWND.
 */
WindowScreenState GetWindowScreenState(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return WindowScreenState::Normal;

    WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
    if (GetWindowPlacement(hwnd, &wp)) {
        if (wp.showCmd == SW_SHOWMAXIMIZED) {
            return WindowScreenState::Maximized;
        }
    }

    RECT winRect;
    if (!GetWindowRect(hwnd, &winRect)) return WindowScreenState::Normal;

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (!GetMonitorInfo(hMonitor, &mi)) return WindowScreenState::Normal;

    const RECT& monRect = mi.rcMonitor;

    bool isCoveringMonitor = (winRect.left   <= monRect.left   &&
                              winRect.top    <= monRect.top    &&
                              winRect.right  >= monRect.right  &&
                              winRect.bottom >= monRect.bottom);

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (isCoveringMonitor && !(style & WS_CAPTION)) {
        return WindowScreenState::Fullscreen;
    }

    const RECT& workRect = mi.rcWork;
    bool isCoveringWorkArea = (winRect.left   <= workRect.left   &&
                               winRect.top    <= workRect.top    &&
                               winRect.right  >= workRect.right  &&
                               winRect.bottom >= workRect.bottom);

    if (isCoveringWorkArea) {
        return WindowScreenState::Maximized;
    }

    return WindowScreenState::Normal;
}

LRESULT CALLBACK MultiWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowRuntime* runtime = reinterpret_cast<WindowRuntime*>(GetPropW(hwnd, L"WINDOW_RUNTIME_PTR"));
    
    if (!runtime) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_CREATE: {
            {
                std::lock_guard<std::mutex> lock(runtime->stateMutex);
                runtime->state.runtime.created = true;
                
                // ⭐ Cập nhật trạng thái Ghim (Always-On-Top) ban đầu
                LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                runtime->state.display.isPinned = (exStyle & WS_EX_TOPMOST) != 0;
            }
            UpdateWindowState(runtime);
            break;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND: {
            return 1;
        }

        case WM_DPICHANGED: {
            float newDpiScale = static_cast<float>(HIWORD(wParam)) / 96.0f;
            {
                std::lock_guard<std::mutex> lock(runtime->stateMutex);
                runtime->state.display.dpiX = HIWORD(wParam);
                runtime->state.display.dpiY = LOWORD(wParam);
                runtime->state.display.dpiScale = newDpiScale;
            }

            // ⭐ Cập nhật DPI Scale sang FontController để Rebuild Font Atlas mượt mà
            if (runtime->fontController) {
                FontSettings fontSettings = runtime->fontController->GetSettings();
                fontSettings.dpiScale = newDpiScale;
                runtime->fontController->UpdateSettings(fontSettings, true /* notifyChildren */);
            }
            
            RECT* const prcNewWindow = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, NULL,
                prcNewWindow->left, prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            break;
        }

        case WM_NCHITTEST: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            if (runtime->state.display.isFullscreen) return HTCLIENT;

            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);

            ResizeEdge edge = MultiWindowDetectEdge(runtime, pt.x, pt.y, rc.right, rc.bottom);
            if (edge != ResizeEdge::NONE) {
                if (runtime->state.display.isMaximized) return HTCLIENT;
                runtime->state.input.resizeEdge = edge;
                switch (edge) {
                    case ResizeEdge::LEFT:        return HTLEFT;
                    case ResizeEdge::RIGHT:       return HTRIGHT;
                    case ResizeEdge::TOP:         return HTTOP;
                    case ResizeEdge::BOTTOM:      return HTBOTTOM;
                    case ResizeEdge::TOPLEFT:     return HTTOPLEFT;
                    case ResizeEdge::TOPRIGHT:    return HTTOPRIGHT;
                    case ResizeEdge::BOTTOMLEFT:  return HTBOTTOMLEFT;
                    case ResizeEdge::BOTTOMRIGHT: return HTBOTTOMRIGHT;
                    default: break;
                }
            } else {
                runtime->state.input.resizeEdge = ResizeEdge::NONE;
            }

            
            if (runtime->style.titlebar && pt.y <= runtime->style.titleHeight) {
                float btnW = runtime->style.btnSize;
                
                if (pt.x >= rc.right - btnW)     return HTCUSTOM_CLOSE;
                if (pt.x >= rc.right - btnW * 2) return runtime->state.display.isMaximized ? HTCUSTOM_RETORE : HTCUSTOM_MAX;
                if (pt.x >= rc.right - btnW * 3) return HTCUSTOM_MIN;
                if (pt.x >= rc.right - btnW * 4) return HTCUSTOM_PIN; 
                
                if (pt.x < rc.right - btnW * 4)  return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_NCCALCSIZE: {
            if (!wParam) break;
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            if (runtime->style.borderless) {
                NCCALCSIZE_PARAMS* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{ sizeof(mi) };
                GetMonitorInfo(mon, &mi);
                
                if (IsZoomed(hwnd)) {
                    p->rgrc[0] = mi.rcWork;
                }
                return 0; 
            }
            break;
        }

        case WM_NCLBUTTONDOWN: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.input.lastHit = wParam;
            if (wParam == HTCUSTOM_CLOSE)   runtime->state.input.mouseDownClose = true;
            if (wParam == HTCUSTOM_MAX)     runtime->state.input.mouseDownMax = true;
            if (wParam == HTCUSTOM_RETORE)  runtime->state.input.mouseDownRestore = true;
            if (wParam == HTCUSTOM_MIN)      runtime->state.input.mouseDownMin = true;
            if (wParam == HTCUSTOM_PIN)      runtime->state.input.mouseDownPin = true; 

            if (wParam >= HTLEFT && wParam <= HTBOTTOMRIGHT) {
                 runtime->state.input.resizing = true;
            }
            if (wParam == HTCAPTION) {
                runtime->state.input.moving = true;
            }

            runtime->state.input.mouseCaptured = true;
            break;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;

            HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(MONITORINFO) };

            if (GetMonitorInfo(hMonitor, &mi)) {
                RECT rcWork = mi.rcWork;
                RECT rcMon  = mi.rcMonitor;

                mmi->ptMaxPosition.x = abs(rcWork.left - rcMon.left);
                mmi->ptMaxPosition.y = abs(rcWork.top - rcMon.top);

                mmi->ptMaxSize.x = rcWork.right - rcWork.left;
                mmi->ptMaxSize.y = rcWork.bottom - rcWork.top;

                {
                    std::lock_guard<std::mutex> lock(runtime->stateMutex);
                    if (runtime->state.geometry.minWidth > 0)  mmi->ptMinTrackSize.x = runtime->state.geometry.minWidth;
                    if (runtime->state.geometry.minHeight > 0) mmi->ptMinTrackSize.y = runtime->state.geometry.minHeight;
                    if (runtime->state.geometry.maxWidth > 0)  mmi->ptMaxTrackSize.x = runtime->state.geometry.maxWidth;
                    if (runtime->state.geometry.maxHeight > 0) mmi->ptMaxTrackSize.y = runtime->state.geometry.maxHeight;
                }
            }
            return 0;
        }

        case WM_NCLBUTTONUP: {
            int hit = 0;

            {
                std::lock_guard<std::mutex> lock(runtime->stateMutex);

                hit = runtime->state.input.lastHit;

                runtime->state.input.mouseDownClose   = false;
                runtime->state.input.mouseDownMax     = false;
                runtime->state.input.mouseDownRestore = false;
                runtime->state.input.mouseDownMin     = false;
                runtime->state.input.mouseDownPin     = false;
                runtime->state.input.lastHit           = 0;
                runtime->state.input.mouseCaptured     = false;
            }

            switch (hit) {
                case HTCUSTOM_CLOSE:
                    if (runtime && runtime->controller) {
                        runtime->controller->Close();
                    }
                    break;

                case HTCUSTOM_MAX:
                case HTCUSTOM_RETORE: {
                    if (IsZoomed(hwnd)) {
                        PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                    } else {
                        PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                    }
                    break;
                }

                case HTCUSTOM_MIN:
                    PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                    break;

                case HTCUSTOM_PIN: 
                    if (runtime && runtime->controller) {
                        runtime->controller->ToggleAlwaysOnTop();
                    }
                    break;
            }

            break;
        }

        case WM_CAPTURECHANGED: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.input.mouseCaptured     = false;
            runtime->state.input.mouseDownClose   = false;
            runtime->state.input.mouseDownMax     = false;
            runtime->state.input.mouseDownRestore = false;
            runtime->state.input.mouseDownMin     = false;
            runtime->state.input.mouseDownPin     = false;
            break;
        }

        case WM_ENTERSIZEMOVE: {
            SetTimer(hwnd, IDT_RENDER_TIMER, 8, NULL); 
            break;
        }

        case WM_EXITSIZEMOVE: {
            KillTimer(hwnd, IDT_RENDER_TIMER);
            {
                std::lock_guard<std::mutex> lock(runtime->stateMutex);
                
                runtime->state.input.moving   = false;
                runtime->state.input.resizing = false;

                RECT rc;
                if (GetWindowRect(hwnd, &rc)) {
                    runtime->state.geometry.x      = rc.left;
                    runtime->state.geometry.y      = rc.top;
                    runtime->state.geometry.width  = rc.right - rc.left;
                    runtime->state.geometry.height = rc.bottom - rc.top;
                }

                GetWindowPlacement(hwnd, &runtime->state.geometry.placement);
            }

            UpdateWindowState(runtime);
            break;
        }

        case WM_WINDOWPOSCHANGED: {
            WINDOWPOS* pos = reinterpret_cast<WINDOWPOS*>(lParam);
            
            // ⭐ Cập nhật kiểm tra trạng thái Fullscreen / Maximized chính xác khi thay đổi vị trí
            if (pos && (!(pos->flags & SWP_NOSIZE) || !(pos->flags & SWP_NOMOVE))) {
                WindowScreenState screenState = GetWindowScreenState(hwnd);
                bool isFull = (screenState == WindowScreenState::Fullscreen);
                bool isMax  = (screenState == WindowScreenState::Maximized);

                {
                    std::lock_guard<std::mutex> lock(runtime->stateMutex);
                    runtime->state.display.isFullscreen = isFull;
                    runtime->state.display.isMaximized  = isMax;

                    // ⭐ Kiểm tra và đồng bộ lại cờ Ghim (WS_EX_TOPMOST)
                    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                    runtime->state.display.isPinned = (exStyle & WS_EX_TOPMOST) != 0;
                }
            }
            break;
        }

        case WM_MOVING: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.input.moving = true;
            break;
        }

        case WM_SIZING: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.input.resizing = true;
            break;
        }

        case WM_MOVE: {
            int newX = (int)(short)LOWORD(lParam);
            int newY = (int)(short)HIWORD(lParam);
            {
                std::lock_guard<std::mutex> lock(runtime->stateMutex);
                runtime->state.geometry.x = newX;
                runtime->state.geometry.y = newY;
                runtime->state.geometry.currentMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            }

            UpdateWindowState(runtime);
            break;
        }

        case WM_SIZE: {
            int newWidth  = LOWORD(lParam);
            int newHeight = HIWORD(lParam);

            {
                std::lock_guard<std::mutex> lock(runtime->stateMutex);
                switch (wParam) {
                    case SIZE_MINIMIZED:
                        runtime->state.display.isMinimized = true;
                        runtime->state.display.isMaximized = false;
                        break;

                    case SIZE_MAXIMIZED:
                        runtime->state.display.isMinimized = false;
                        runtime->state.display.isMaximized = true;
                        
                        runtime->state.geometry.width  = newWidth;
                        runtime->state.geometry.height = newHeight;
                        break;

                    case SIZE_RESTORED:
                        runtime->state.display.isMinimized = false;
                        runtime->state.display.isMaximized = false;

                        runtime->state.geometry.width  = newWidth;
                        runtime->state.geometry.height = newHeight;
                            
                        runtime->state.geometry.restoreW = newWidth;
                        runtime->state.geometry.restoreH = newHeight;
                        break;
                }
            }
            UpdateWindowState(runtime);
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_RENDER_TIMER) {
                auto& winManager = WindowManager::GetInstance();
                winManager.ForEachWindow([](WindowRuntime *window){
                    if (window && window->resource.GetPlayerSession()) {
                        if (auto* commander = window->resource.GetPlayerSession()->GetCommander()) {
                            commander->Update();
                        }
                    }

                    AdjustWindowFrameRates(window);

                    if (window && window->resource.uiRenderThread && window->state.display.isVisible) {
                        window->resource.uiRenderThread->RequestRender();
                    }
                });
            }
            break;
        }

        case WM_SHOWWINDOW: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.display.isShown = (BOOL)wParam;
            runtime->state.display.isVisible = (BOOL)wParam;
            break;
        }

        case WM_ACTIVATE: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.input.isActive = (LOWORD(wParam) != WA_INACTIVE);
            break;
        }

        case WM_SETFOCUS: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.input.hasFocus = true;
            break;
        }

        case WM_KILLFOCUS: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.input.hasFocus = false;
            break;
        }

        case WM_CLOSE: {
            bool isClosedPending = true;

            {
                std::lock_guard<std::mutex> lock(runtime->stateMutex);
                if (!runtime->state.runtime.isClosedPending) {
                    isClosedPending = runtime->state.runtime.isClosedPending;
                    runtime->state.runtime.isClosedPending = true;
                }
            }

            if (!isClosedPending) {
                if (runtime->controller) runtime->controller->Close();
            }
            return 0;
        }

        case WM_DESTROY: {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.runtime.destroyed = true;
            break;
        }
    }

    WNDPROC oldWndProc = runtime->properties.GetValue<WNDPROC>("OldWndProc", DefWindowProcW);
    return CallWindowProcW(oldWndProc, hwnd, msg, wParam, lParam);
}