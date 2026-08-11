// WindowProcHook.cpp
#include "WindowRuntime.h"
#include "MainWindowState.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include "UpdateWindowState.h"
#include "WindowResource.h"
#include "WindowManager.h"
#include "UIRenderThread.h"

#include "player/session/PlayerSession.h"

#include <windowsx.h>

#define HTCUSTOM_MIN 1000
#define HTCUSTOM_MAX 1001
#define HTCUSTOM_CLOSE 1002
#define HTCUSTOM_RETORE 1003
#define _WIN32_WINNT 0x0A00
#define IDT_RENDER_TIMER 1004

// Hàm detect góc cạnh để resize kế thừa từ logic cũ của bạn
static ResizeEdge MultiWindowDetectEdge(WindowRuntime* runtime, int localX, int localY, int winW, int winH) {
    if (runtime->state.display.isMaximized || runtime->state.display.isFullscreen) 
        return ResizeEdge::NONE;

    int margin = runtime->style.resizeMargin;
    const bool left   = (localX <= margin);
    const bool right  = (localX >= winW - margin);
    const bool top    = (localY <= margin);
    const bool bottom = (localY >= winH - margin);

    if (top && left) return ResizeEdge::TOPLEFT;
    if (top && right) return ResizeEdge::TOPRIGHT;
    if (bottom && left) return ResizeEdge::BOTTOMLEFT;
    if (bottom && right) return ResizeEdge::BOTTOMRIGHT;
    if (left) return ResizeEdge::LEFT;
    if (right) return ResizeEdge::RIGHT;
    if (top) return ResizeEdge::TOP;
    if (bottom) return ResizeEdge::BOTTOM;
    return ResizeEdge::NONE;
}

LRESULT CALLBACK MultiWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Trích xuất instance WindowRuntime của cửa sổ hiện tại
    WindowRuntime* runtime = reinterpret_cast<WindowRuntime*>(GetPropW(hwnd, L"WINDOW_RUNTIME_PTR"));
    
    if (!runtime) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_CREATE: {
            runtime->state.runtime.created = true;
            runtime->state.runtime.is_dirty = RouteWindowStateUpdate(runtime);
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

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            if (runtime->state.geometry.minWidth > 0) mmi->ptMinTrackSize.x = runtime->state.geometry.minWidth;
            if (runtime->state.geometry.minHeight > 0) mmi->ptMinTrackSize.y = runtime->state.geometry.minHeight;
            if (runtime->state.geometry.maxWidth > 0) mmi->ptMaxTrackSize.x = runtime->state.geometry.maxWidth;
            if (runtime->state.geometry.maxHeight > 0) mmi->ptMaxTrackSize.y = runtime->state.geometry.maxHeight;
            return 0;
        }

        case WM_DPICHANGED: {
            runtime->state.display.dpiX = HIWORD(wParam);
            runtime->state.display.dpiY = LOWORD(wParam);
            runtime->state.display.dpiScale = static_cast<float>(HIWORD(wParam)) / 96.0f;
            
            RECT* const prcNewWindow = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, NULL,
                prcNewWindow->left, prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            break;
        }

        case WM_NCHITTEST: {
            if (runtime->state.display.isFullscreen) return HTCLIENT;

            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);

            ResizeEdge edge = MultiWindowDetectEdge(runtime, pt.x, pt.y, rc.right, rc.bottom);
            if (edge != ResizeEdge::NONE) {
                if (runtime->state.display.isMaximized) return HTCLIENT;
                runtime->state.input.resizeEdge = edge; // Update state
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
                if (pt.x >= rc.right - runtime->style.btnSize) return HTCUSTOM_CLOSE;
                if (pt.x >= rc.right - runtime->style.btnSize * 2) return runtime->state.display.isMaximized ? HTCUSTOM_RETORE : HTCUSTOM_MAX;
                if (pt.x >= rc.right - runtime->style.btnSize * 3) return HTCUSTOM_MIN;
                
                if (pt.x < rc.right - runtime->style.btnSize * 3) return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_NCCALCSIZE: {
            if (!wParam) break;
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
            runtime->state.input.lastHit = wParam;
            if (wParam == HTCUSTOM_CLOSE) runtime->state.input.mouseDownClose = true;
            if (wParam == HTCUSTOM_MAX) runtime->state.input.mouseDownMax = true;
            if (wParam == HTCUSTOM_RETORE) runtime->state.input.mouseDownRestore = true;
            if (wParam == HTCUSTOM_MIN) runtime->state.input.mouseDownMin = true;

            if (wParam >= HTLEFT && wParam <= HTBOTTOMRIGHT) {
                 runtime->state.input.resizing = true;
            }
            if (wParam == HTCAPTION) {
                runtime->state.input.moving = true;
            }

            //SetCapture(hwnd);
            runtime->state.input.mouseCaptured = true;
            break;
        }

        case WM_NCLBUTTONUP: {
            if (wParam == runtime->state.input.lastHit) {
                if (wParam == HTCUSTOM_CLOSE) runtime->controller->Close();
                if (wParam == HTCUSTOM_MAX) SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                if (wParam == HTCUSTOM_RETORE) SendMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                if (wParam == HTCUSTOM_MIN) SendMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
            }
            runtime->state.input.mouseDownClose = false;
            runtime->state.input.mouseDownMax = false;
            runtime->state.input.mouseDownRestore = false;
            runtime->state.input.mouseDownMin = false;
            runtime->state.input.lastHit = 0;

            //ReleaseCapture();
            runtime->state.input.mouseCaptured = false;
            break;
        }

        case WM_CAPTURECHANGED:
            runtime->state.input.mouseCaptured = false;
            runtime->state.input.mouseDownClose = false;
            runtime->state.input.mouseDownMax = false;
            runtime->state.input.mouseDownRestore = false;
            runtime->state.input.mouseDownMin = false;
            break;

        case WM_ENTERSIZEMOVE:
            SetTimer(hwnd, IDT_RENDER_TIMER, 16, NULL); 
            break;

        case WM_EXITSIZEMOVE:
            KillTimer(hwnd, IDT_RENDER_TIMER);
            runtime->state.input.moving = false;
            runtime->state.input.resizing = false;
            break;

        case WM_MOVING:
            runtime->state.input.moving = true;
            break;
        
        case WM_SIZING:
            runtime->state.input.resizing = true;
            break;

        case WM_TIMER:
            if (wParam == IDT_RENDER_TIMER) {
                auto& winManager = WindowManager::GetInstance();
                runtime->state.runtime.is_dirty = RouteWindowStateUpdate(runtime);

                if (auto* commamder = runtime->resource.GetPlayerSession()->GetCommander())
                    commamder->Update();

                AdjustWindowFrameRates(winManager);

                if (runtime && runtime->resource.uiRenderThread && runtime->state.display.isVisible) {
                    runtime->resource.uiRenderThread->RequestRender();
                }
            }
            break;

        case WM_SIZE:
        {
            switch (wParam)
            {
            case SIZE_MINIMIZED:
                runtime->state.display.isMinimized = true;
                runtime->state.display.isMaximized = false;
                break;
            case SIZE_MAXIMIZED:
                runtime->state.display.isMaximized = true;
                runtime->state.display.isMinimized = false;
                break;
            case SIZE_RESTORED:
                runtime->state.display.isMinimized = false;
                runtime->state.display.isMaximized = false;
                break;
            }
            break;
        }

        case WM_SHOWWINDOW:
        {
            runtime->state.display.isShown = (BOOL)wParam;
            break;
        }

        case WM_ACTIVATE:
        {
            runtime->state.input.isActive = (LOWORD(wParam) != WA_INACTIVE);
            break;
        }

        case WM_SETFOCUS:
            runtime->state.input.hasFocus = true;
            break;

        case WM_KILLFOCUS:
            runtime->state.input.hasFocus = false;
            break;

        case WM_CLOSE:
            if (!runtime->state.runtime.isClosedPending) {
                runtime->state.runtime.isClosedPending = true;
                if(runtime->controller) runtime->controller->Close();
            }
            return 0; // Ngăn DefWindowProc phá hủy cửa sổ ngay lập tức

        case WM_DESTROY:
            runtime->state.runtime.destroyed = true;
            // TODO: Thông báo cho WindowManager để dọn dẹp
            break;
    }

    WNDPROC oldWndProc = runtime->properties.GetValue<WNDPROC>("OldWndProc", DefWindowProcW);
    return CallWindowProcW(oldWndProc, hwnd, msg, wParam, lParam);
}