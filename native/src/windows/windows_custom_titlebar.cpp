// ================= Borderless WinAPI + SDL sync (full) ===================
// yêu cầu: Windows + SDL2 + ImGui
#define HTCUSTOM_MIN 1000
#define HTCUSTOM_MAX 1001
#define HTCUSTOM_CLOSE 1002
#define HTCUSTOM_RETORE 1003
#define _WIN32_WINNT 0x0A00


#include "globals.h"
#include "utils.h"
#include "imgui.h"

#include <windows/windows_custom_titlebar.h>
#include <windows/windows_borderless_state.h>
#include <windows/windows_borderless.h>

#include <threads/thread.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <windowsx.h>
#include <algorithm> 
#include <dwmapi.h>

#include <shellapi.h>

#define WM_TRAYICON (WM_USER + 1)

NOTIFYICONDATA nid = {};


DragResizeState g_DragResizeState;
// -------------------- Helpers --------------------
static ResizeEdge DetectResizeEdge(int localX, int localY, int winW, int winH, int margin) {

    if (g_DragResizeState.IsMax || g_DragResizeState.IsFullscreen_video) 
        return ResizeEdge::NONE; // khóa resize khi đang max

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
};
static inline void AddTrayIcon(HWND hwnd)
{
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;

    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;

    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"My App Running");

    Shell_NotifyIcon(NIM_ADD, &nid);
};
static inline void RemoveTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &nid);
};

// -------------------- WndProc hook: min/max từ SDL --------------------
LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, SDL_Window* sdlWindow)
{
    switch (msg) {
        // ===== Giới hạn kích thước, aspect ratio =====
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            if (g_DragResizeState.sdlMinW > 0 && g_DragResizeState.sdlMinH > 0) {
                mmi->ptMinTrackSize.x = g_DragResizeState.sdlMinW;
                mmi->ptMinTrackSize.y = g_DragResizeState.sdlMinH;
            }
            if (g_DragResizeState.sdlMaxW > 0 && g_DragResizeState.sdlMaxH > 0) {
                mmi->ptMaxTrackSize.x = g_DragResizeState.sdlMaxW;
                mmi->ptMaxTrackSize.y = g_DragResizeState.sdlMaxH;
            }
            return 0;
        }
        case WM_APPCOMMAND: 
        {
            int cmd = GET_APPCOMMAND_LPARAM(lParam);
            switch (cmd) 
            {
                case APPCOMMAND_MEDIA_PLAY_PAUSE:
                {
                    const char *c[] = {"cycle", "pause", NULL};
                    mpv_command_async(mpv.mpv, 0, c);
                    break;
                }
                case APPCOMMAND_MEDIA_NEXTTRACK:
                {
                    const char *cmd_next[] = { "playlist-next", NULL };
                    mpv_command_async(mpv.mpv, 0, cmd_next);
                    break;
                }
                case APPCOMMAND_MEDIA_PREVIOUSTRACK:
                {
                    const char *cmd_prev[] = { "playlist-prev", NULL };
                    mpv_command_async(mpv.mpv, 0, cmd_prev);
                    break;
                }
                case APPCOMMAND_VOLUME_UP:{}
                case APPCOMMAND_VOLUME_DOWN:{}
            }
            return 0;
        }
        case WM_COPYDATA: {
            PCOPYDATASTRUCT pCDS = (PCOPYDATASTRUCT)lParam;
            std::string url((char*)pCDS->lpData, pCDS->cbData);
            CallThread_URLFetch(url, true); // append vào playlist
            return 0;
        }

        // ===== HitTest & chuột =====
        case WM_NCHITTEST: {

            if(g_DragResizeState.IsFullscreen_video) {
                g_DragResizeState.debugInfo = "HTCLIENT (Max/FS)";
                return HTCLIENT;
            }
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);

            ResizeEdge edge = DetectResizeEdge(pt.x, pt.y,
                                            rc.right, rc.bottom,
                                            g_DragResizeState.resizeMargin);

            
            if (edge != ResizeEdge::NONE) {
                if (g_DragResizeState.IsMax) {
                    g_DragResizeState.debugInfo = "HTCLIENT (Max/FS)";
                    return HTCLIENT;
                }
                switch (edge) {
                    case ResizeEdge::LEFT:
                        g_DragResizeState.debugInfo = "HTLEFT";
                        return HTLEFT;
                    case ResizeEdge::RIGHT:
                        g_DragResizeState.debugInfo = "HTRIGHT";
                        return HTRIGHT;
                    case ResizeEdge::TOP:
                        g_DragResizeState.debugInfo = "HTTOP";
                        return HTTOP;
                    case ResizeEdge::BOTTOM:
                        g_DragResizeState.debugInfo = "HTBOTTOM";
                        return HTBOTTOM;
                    case ResizeEdge::TOPLEFT:
                        g_DragResizeState.debugInfo = "HTTOPLEFT";
                        return HTTOPLEFT;
                    case ResizeEdge::TOPRIGHT:
                        g_DragResizeState.debugInfo = "HTTOPRIGHT";
                        return HTTOPRIGHT;
                    case ResizeEdge::BOTTOMLEFT:
                        g_DragResizeState.debugInfo = "HTBOTTOMLEFT";
                        return HTBOTTOMLEFT;
                    case ResizeEdge::BOTTOMRIGHT:
                        g_DragResizeState.debugInfo = "HTBOTTOMRIGHT";
                        return HTBOTTOMRIGHT;
                }
            }
            
            // Control buttons
            if (pt.y <= g_DragResizeState.TitleHeight) {
                
                // 1. Kiểm tra các nút điều khiển (tính từ phải sang trái)
                if (pt.x >= rc.right - g_DragResizeState.btnSize) {
                    g_DragResizeState.debugInfo = "HTCUSTOM_CLOSE";
                    return HTCUSTOM_CLOSE;
                } 
                else if (pt.x >= rc.right - g_DragResizeState.btnSize * 2) {
                    if (g_DragResizeState.IsMax) {
                        g_DragResizeState.debugInfo = "HTCUSTOM_RETORE";
                        return HTCUSTOM_RETORE;
                    } else {
                        g_DragResizeState.debugInfo = "HTCUSTOM_MAX";
                        return HTCUSTOM_MAX;
                    }
                } 
                else if (pt.x >= rc.right - g_DragResizeState.btnSize * 3) {
                    g_DragResizeState.debugInfo = "HTCUSTOM_MIN";
                    return HTCUSTOM_MIN;
                }

                // 2. Nếu không trúng nút, kiểm tra xem có nằm trong vùng Title bar không
                // (Vùng còn lại bên trái các nút bấm)
                if (pt.x < rc.right - g_DragResizeState.btnSize * 3) {
                    g_DragResizeState.debugInfo = "HTCAPTION";
                    return HTCAPTION;
                }
            }

            g_DragResizeState.debugInfo = "HTCLIENT";
            return HTCLIENT;
        }

        case WM_NCCALCSIZE:
        {   
            
            if (!wParam) break;
            
            NCCALCSIZE_PARAMS* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
            
            // Kiểm tra fullscreen
            RECT winRect = p->rgrc[0];
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{ sizeof(mi) };
            GetMonitorInfo(mon, &mi);
            bool isFullscreen = EqualRect(&winRect, &mi.rcMonitor);

            if (isFullscreen) {
                // Fullscreen → dùng rect monitor luôn, không trừ border
                p->rgrc[0] = mi.rcMonitor;
            }
            else if (IsZoomed(hwnd)) {
                // Maximize → loại bỏ viền vô hình (work area)
                p->rgrc[0] = mi.rcWork;
            }
            else {
            
            }
            return 0;
        }
        
        
        case WM_NCLBUTTONDOWN: {
            // wParam chính là hit-test code
            LRESULT hit = wParam;
            g_DragResizeState.lastHit = wParam;

            if (hit == HTCUSTOM_CLOSE) g_DragResizeState.mouseDownClose = true;
            else if (hit == HTCUSTOM_MAX) g_DragResizeState.mouseDownMax = true;
            else if (hit == HTCUSTOM_RETORE) g_DragResizeState.mouseDownRestore = true;
            else if ( hit == HTCUSTOM_MIN) g_DragResizeState.mouseDownMin = true;

            break;
        }


        case WM_NCLBUTTONUP:
        {
            LRESULT hit = wParam;

            if(hit == g_DragResizeState. lastHit){
                if (hit == HTCUSTOM_CLOSE) SendMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
                else if (hit == HTCUSTOM_MAX) SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                else if (hit == HTCUSTOM_RETORE) SendMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                else if ( hit == HTCUSTOM_MIN) SendMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);  
            }

            g_DragResizeState.mouseDownClose = false;
            g_DragResizeState.mouseDownMax = false;
            g_DragResizeState.mouseDownRestore = false;
            g_DragResizeState.mouseDownMin = false;

            g_DragResizeState.lastHit = 0;
            ReleaseCapture();
            
            break;
        }
        
        case WM_LBUTTONUP:
        {
            g_DragResizeState.mouseDownClose = false;
            g_DragResizeState.mouseDownMax = false;
            g_DragResizeState.mouseDownRestore = false;
            g_DragResizeState.mouseDownMin = false;

            break;
        }
        
        case WM_DPICHANGED: {
            g_DragResizeState.dpiX = LOWORD(wParam);
            g_DragResizeState.dpiY = HIWORD(wParam);
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, prc->left, prc->top,
                         prc->right - prc->left, prc->bottom - prc->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            if (sdlWindow) {
                SyncSDLWithWinAPI(sdlWindow);
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_SIZE_CHANGED,
                               prc->right - prc->left, prc->bottom - prc->top);
            }
            return 0;
        }

 
        case WM_TRAYICON:
        {
            switch (lParam)
            {
                case WM_LBUTTONDBLCLK:
                    ShowWindow(hwnd, SW_SHOW);
                    SetForegroundWindow(hwnd);
                    break;

                case WM_RBUTTONUP:
                {
                    HMENU menu = CreatePopupMenu();
                    AppendMenu(menu, MF_STRING, 1, L"Show");
                    AppendMenu(menu, MF_STRING, 2, L"Exit");

                    POINT pt;
                    GetCursorPos(&pt);
                    SetForegroundWindow(hwnd);

                    int cmd = TrackPopupMenu(menu,
                                            TPM_RETURNCMD | TPM_NONOTIFY,
                                            pt.x, pt.y,
                                            0, hwnd, NULL);

                    if (cmd == 1) {
                        ShowWindow(hwnd, SW_SHOW);
                    }
                    else if (cmd == 2) {
                        RemoveTrayIcon();
                        if (sdlWindow) SDLX_PushClose(sdlWindow);
                        PostQuitMessage(0);

                    }

                    DestroyMenu(menu);
                }
                break;
            }
        }
        break;
        case WM_CLOSE:{

            if(g_DragResizeState.trayiconAdded){
                ShowWindow(hwnd, SW_HIDE);
                AddTrayIcon(hwnd);
                return 0;
            }else{
                RemoveTrayIcon();
                if (sdlWindow) SDLX_PushClose(sdlWindow);
                return 0;
            }
        break;
        }

    }

    return CallWindowProc(g_DragResizeState.g_OldWndProc ?
                          g_DragResizeState.g_OldWndProc : DefWindowProc,
                          hwnd, msg, wParam, lParam);
};


void SDLX_InitBorderless(SDL_Window* window)
{
    if (!window) return;
    
    // Lấy HWND
    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) return;
    HWND hwnd = wmInfo.info.win.window;

    g_DragResizeState.hwnd_windown_main = hwnd;

    // Lưu SDL_Window để WndProc lấy lại
    SetProp(hwnd, L"SDLWIN", (HANDLE)window);
    
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style |= (  WS_MINIMIZEBOX  |
                  WS_THICKFRAME |
                    WS_CAPTION 
    );
    if (g_DragResizeState.snapEnabled )
    {
        style |= ( WS_MAXIMIZEBOX );
    }
    g_DragResizeState.style = style;
    SetWindowLong(hwnd, GWL_STYLE, style);

    SetWindowPos(hwnd, NULL, 0,0,0,0,
                SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

    // Hook WndProc nếu chưa
    if (!g_DragResizeState.g_OldWndProc) {
        g_DragResizeState.g_OldWndProc = (WNDPROC)SetWindowLongPtr(
            hwnd, GWLP_WNDPROC,
            (LONG_PTR)+[](HWND h, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                SDL_Window* sdlWin = (SDL_Window*)GetProp(h, L"SDLWIN"); // Dùng h thay vì hwnd
                return CustomWndProc(h, msg, wParam, lParam, sdlWin);
            }
        );
    }
};




