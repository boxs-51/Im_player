// WindowProcHook.cpp
#include "WindowRuntime.h"
#include "MainWindowState.h"
#include "mpv_render_video.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <windowsx.h>

#define HTCUSTOM_MIN 1000
#define HTCUSTOM_MAX 1001
#define HTCUSTOM_CLOSE 1002
#define HTCUSTOM_RETORE 1003
#define _WIN32_WINNT 0x0A00
#define IDT_RENDER_TIMER 1004

// Hàm detect góc cạnh để resize kế thừa từ logic cũ của bạn
static ResizeEdge1 MultiWindowDetectEdge(WindowRuntime* runtime, int localX, int localY, int winW, int winH) {
    if (runtime->state.isMaximized || runtime->state.isFullscreen) 
        return ResizeEdge1::NONE;

    int margin = runtime->style.resizeMargin;
    const bool left   = (localX <= margin);
    const bool right  = (localX >= winW - margin);
    const bool top    = (localY <= margin);
    const bool bottom = (localY >= winH - margin);

    if (top && left) return ResizeEdge1::TOPLEFT;
    if (top && right) return ResizeEdge1::TOPRIGHT;
    if (bottom && left) return ResizeEdge1::BOTTOMLEFT;
    if (bottom && right) return ResizeEdge1::BOTTOMRIGHT;
    if (left) return ResizeEdge1::LEFT;
    if (right) return ResizeEdge1::RIGHT;
    if (top) return ResizeEdge1::TOP;
    if (bottom) return ResizeEdge1::BOTTOM;
    return ResizeEdge1::NONE;
}

LRESULT CALLBACK MultiWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Trích xuất instance WindowRuntime của cửa sổ hiện tại
    WindowRuntime* runtime = reinterpret_cast<WindowRuntime*>(GetPropW(hwnd, L"WINDOW_RUNTIME_PTR"));
    
    if (!runtime) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            if (runtime->state.minWidth > 0) mmi->ptMinTrackSize.x = runtime->state.minWidth;
            if (runtime->state.minHeight > 0) mmi->ptMinTrackSize.y = runtime->state.minHeight;
            if (runtime->state.maxWidth > 0) mmi->ptMaxTrackSize.x = runtime->state.maxWidth;
            if (runtime->state.maxHeight > 0) mmi->ptMaxTrackSize.y = runtime->state.maxHeight;
            return 0;
        }

        case WM_NCHITTEST: {
            if (runtime->state.isFullscreen) return HTCLIENT;

            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);

            ResizeEdge1 edge = MultiWindowDetectEdge(runtime, pt.x, pt.y, rc.right, rc.bottom);
            if (edge != ResizeEdge1::NONE) {
                if (runtime->state.isMaximized) return HTCLIENT;
                switch (edge) {
                    case ResizeEdge1::LEFT:        return HTLEFT;
                    case ResizeEdge1::RIGHT:       return HTRIGHT;
                    case ResizeEdge1::TOP:         return HTTOP;
                    case ResizeEdge1::BOTTOM:      return HTBOTTOM;
                    case ResizeEdge1::TOPLEFT:     return HTTOPLEFT;
                    case ResizeEdge1::TOPRIGHT:    return HTTOPRIGHT;
                    case ResizeEdge1::BOTTOMLEFT:  return HTBOTTOMLEFT;
                    case ResizeEdge1::BOTTOMRIGHT: return HTBOTTOMRIGHT;
                    default: break;
                }
            }

            // Custom Titlebar và các nút điều khiển dựa trên Style cụ thể của từng cửa sổ
            if (runtime->style.titlebar && pt.y <= runtime->style.titleHeight) {
                if (pt.x >= rc.right - runtime->style.btnSize) return HTCUSTOM_CLOSE; // Close
                if (pt.x >= rc.right - runtime->style.btnSize * 2) return runtime->state.isMaximized ? HTCUSTOM_RETORE : HTCUSTOM_MAX; // Max/Restore
                if (pt.x >= rc.right - runtime->style.btnSize * 3) return HTCUSTOM_MIN; // Min
                
                if (pt.x < rc.right - runtime->style.btnSize * 3) return HTCAPTION; // Kéo cửa sổ
            }
            return HTCLIENT;
        }

        case WM_NCCALCSIZE: {
            if (!wParam) break;
            NCCALCSIZE_PARAMS* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
            if (runtime->style.borderless) {
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{ sizeof(mi) };
                GetMonitorInfo(mon, &mi);
                
                if (IsZoomed(hwnd)) {
                    p->rgrc[0] = mi.rcWork; // Maximize không đè lên Taskbar
                } else {
                    p->rgrc[0] = p->rgrc[0]; // Giữ nguyên kích thước chuẩn borderless
                }
                return 0;
            }
            break;
        }

        case WM_NCLBUTTONDOWN: {
            runtime->state.lastHit = wParam;
            if (wParam == HTCUSTOM_CLOSE) runtime->state.mouseDownClose = true;
            if (wParam == HTCUSTOM_MAX) runtime->state.mouseDownMax = true;
            if (wParam == HTCUSTOM_RETORE) runtime->state.mouseDownRestore = true;
            if (wParam == HTCUSTOM_MIN) runtime->state.mouseDownMin = true;
            break;
        }

        case WM_NCLBUTTONUP: {
            if (wParam == runtime->state.lastHit) {
                if (wParam == HTCUSTOM_CLOSE) runtime->controller->Close();
                if (wParam == HTCUSTOM_MAX) SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                if (wParam == HTCUSTOM_RETORE) SendMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                if (wParam == HTCUSTOM_MIN) SendMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
            }
            runtime->state.mouseDownClose = false;
            runtime->state.mouseDownMax = false;
            runtime->state.mouseDownRestore = false;
            runtime->state.mouseDownMin = false;
            runtime->state.lastHit = 0;
            ReleaseCapture();
            break;
        }
        case WM_ENTERSIZEMOVE:
            // Bắt đầu timer khi người dùng bắt đầu nhấn giữ để kéo/nắm cửa sổ
            SetTimer(hwnd, IDT_RENDER_TIMER, 16, NULL); 
            break;
        case WM_EXITSIZEMOVE:
            // Tắt timer khi người dùng thả chuột để tiết kiệm tài nguyên
            KillTimer(hwnd, IDT_RENDER_TIMER);
            break;

        case WM_TIMER:
        if (wParam == IDT_RENDER_TIMER) {
            // Cập nhật layout và vẽ ngay lập tức
            UpdateWindowState(runtime);
            if (!runtime->state.isVisible) break; // Bỏ qua nếu cửa sổ bị ẩn
            if (runtime->style.isMainWindow){
                #ifdef RENDER_MPV_THREAD
                    // 1. Đồng bộ trạng thái Ẩn/Hiện vật lý sang cho Luồng Render MPV phụ
                    renderThread.g_WindowVisible.store(runtime->state.isVisible, std::memory_order_relaxed);

                    // 2. Đồng bộ kích thước hình học mới nếu người dùng đang kéo co giãn cửa sổ
                    if (runtime->properties.Contains("Layout")) {
                        auto layout = runtime->properties.Get<MainWindowLayout>("Layout");
                        
                        std::lock_guard<std::mutex> lock(renderThread.mtx);
                        if (renderThread.surface.drawW != (int)layout.VideoSize.x || 
                            renderThread.surface.drawH != (int)layout.VideoSize.y) {
                            
                            renderThread.surface.newW = (int)layout.VideoSize.x;
                            renderThread.surface.newH = (int)layout.VideoSize.y;
                            renderThread.surface.needResize = true;
                        }
                    }
                #endif
            }
            // Chuyển đổi ngữ cảnh ImGui chính xác cho cửa sổ đang vẽ
            ImGuiContext* ctxOfWindow = runtime->properties.Get<ImGuiContext*>("ImGuiCtx");
            if (ctxOfWindow) ImGui::SetCurrentContext(ctxOfWindow);

            if (runtime->renderer) {
                runtime->renderer->BeginFrame();
                runtime->renderer->RenderUI(runtime);
                runtime->renderer->EndFrame();
            }
            
            SDL_GL_SwapWindow(runtime->sdlWindow);

        }
        break;

        case WM_SIZE: {
            runtime->state.isMaximized = (wParam == SIZE_MAXIMIZED);
            runtime->state.isMinimized = (wParam == SIZE_MINIMIZED);
            break;
        }
    }

    WNDPROC oldWndProc = runtime->properties.Get<WNDPROC>("OldWndProc", DefWindowProcW);
    return CallWindowProcW(oldWndProc, hwnd, msg, wParam, lParam);
}