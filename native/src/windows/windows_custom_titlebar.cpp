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

#include "windows/windows_custom_titlebar.h"
#include "windows/windows_borderless_state.h"
#include "windows/windows_borderless.h"

#include "thread.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <windowsx.h>
#include <algorithm> 
#include <dwmapi.h>

#include <shellapi.h>

#define WM_TRAYICON (WM_USER + 1)

NOTIFYICONDATA nid = {};

//static std::unique_ptr<DWMAnimator> g_animator;
DragResizeState g_DragResizeState{};
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
}
static HCURSOR GetCursorForEdge(ResizeEdge edge) {
    switch(edge) {
        case ResizeEdge::LEFT:
        case ResizeEdge::RIGHT:        return LoadCursor(NULL, IDC_SIZEWE);
        case ResizeEdge::TOP:
        case ResizeEdge::BOTTOM:       return LoadCursor(NULL, IDC_SIZENS);
        case ResizeEdge::TOPLEFT:
        case ResizeEdge::BOTTOMRIGHT:  return LoadCursor(NULL, IDC_SIZENWSE);
        case ResizeEdge::TOPRIGHT:
        case ResizeEdge::BOTTOMLEFT:   return LoadCursor(NULL, IDC_SIZENESW);
        default:                       return LoadCursor(NULL, IDC_ARROW);
    }
}
struct EnumMonCtx { int want; int idx; HMONITOR result; };
static BOOL CALLBACK EnumMonProc(HMONITOR hMon, HDC, LPRECT, LPARAM lp)
{
    auto* ctx = (EnumMonCtx*)lp;
    if (ctx->idx == ctx->want) { ctx->result = hMon; return FALSE; }
    ctx->idx++; return TRUE;
}
static inline bool SDLX_MoveWindowToMonitor(SDL_Window* window, int monitorIndex, bool center)
{
    if (!window) return false;
    SDL_SysWMinfo wmInfo{}; SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) return false;
    HWND hwnd = wmInfo.info.win.window;

    EnumMonCtx ctx{ monitorIndex, 0, nullptr };
    EnumDisplayMonitors(NULL, NULL, EnumMonProc, (LPARAM)&ctx);
    if (!ctx.result) return false;

    MONITORINFO mi{ sizeof(MONITORINFO) };
    if (!GetMonitorInfo(ctx.result, &mi)) return false;
    RECT wa = mi.rcWork;

    RECT rc; GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    int x = center ? wa.left + ((wa.right - wa.left) - w) / 2 : wa.left;
    int y = center ? wa.top  + ((wa.bottom - wa.top) - h) / 2 : wa.top;

    SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);
    SDL_SetWindowPosition(window, x, y);
    return true;
}
static inline void SDLX_SetTitlebarMetrics(int titleHeight, int resizeMargin)
{
    g_DragResizeState.TitleHeight  = titleHeight;
    g_DragResizeState.resizeMargin = resizeMargin;
}
static inline void KeepAspectRatio(RECT* rc, int edge)
{
    if (!g_DragResizeState.aspectLock || g_DragResizeState.aspectDen == 0) return;

    int  num = g_DragResizeState.aspectNum;
    int  den = g_DragResizeState.aspectDen;
    LONG w = rc->right - rc->left;
    LONG h = rc->bottom - rc->top;

    // Tính theo bề rộng, cập nhật chiều cao cho khớp tỉ lệ
    LONG targetH = (LONG)((double)w * den / num + 0.5);

    switch (edge) {
        // resize theo chiều ngang → chỉnh lại height
        case WMSZ_LEFT: case WMSZ_RIGHT:
        case WMSZ_TOPLEFT: case WMSZ_TOPRIGHT:
        case WMSZ_BOTTOMLEFT: case WMSZ_BOTTOMRIGHT: {
            if (edge == WMSZ_TOPLEFT || edge == WMSZ_TOP || edge == WMSZ_TOPRIGHT)
                rc->top = rc->bottom - targetH;
            else
                rc->bottom = rc->top + targetH;
            break;
        }
        // resize theo chiều dọc → chỉnh lại width
        case WMSZ_TOP: case WMSZ_BOTTOM: {
            LONG targetW = (LONG)((double)h * num / den + 0.5);
            if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM)
                rc->right = rc->left + targetW;
            break;
        }
    }
}
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
}
static inline void RemoveTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

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
        case WM_CREATE:
        {   
            BOOL disable = FALSE;
            DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disable, sizeof(disable));
            
            MARGINS m = {0,0,0,0};
            DwmExtendFrameIntoClientArea(hwnd, &m);

            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
            //SyncSDLWithWinAPI(sdlWindow);

            //g_animator = std::make_unique<DWMAnimator>();
            //g_animator->Init(hwnd);
            
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
            return TRUE;
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
            if (PtInRect(&g_DragResizeState.rcClose, pt)) {
                g_DragResizeState.debugInfo = "HTCUSTOM_CLOSE";
                return HTCUSTOM_CLOSE;
            }
            if (PtInRect(&g_DragResizeState.rcMax, pt)) {
                if (g_DragResizeState.IsMax) {
                    g_DragResizeState.debugInfo = "HTCUSTOM_RETORE";
                    return HTCUSTOM_RETORE; // 
                } else {
                    g_DragResizeState.debugInfo = "HTCUSTOM_MAX";
                    return HTCUSTOM_MAX; // maximize
                }
            }
            if (PtInRect(&g_DragResizeState.rcMin, pt)) {
                g_DragResizeState.debugInfo = "HTCUSTOM_MIN";
                return HTCUSTOM_MIN;
            }

            // Title bar
            if (pt.y - rc.top <= g_DragResizeState.TitleHeight &&
                pt.x - rc.left < g_DragResizeState.ControlWidth) {
                g_DragResizeState.debugInfo = "HTCAPTION";
                return HTCAPTION;
            }

            g_DragResizeState.debugInfo = "HTCLIENT";
            return HTCLIENT;
        }
        
        //case WM_NCPAINT:
        //    return 0; 
        //case WM_NCACTIVATE:
        //    return 0; 
        
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
                
                /*
                UINT dpi = GetDpiForWindow(hwnd);

                int frameX = GetSystemMetricsForDpi(SM_CXFRAME, dpi)
                        + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

                int frameY = GetSystemMetricsForDpi(SM_CYFRAME, dpi)
                        + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi); // FIX

                */
                InflateRect(&p->rgrc[0],0, 0);
                
                //p->rgrc[0] = p->rgrc[0];
            }
            
            return 0;
        }

        
        case WM_NCLBUTTONDOWN: {
            // wParam chính là hit-test code
            
            LRESULT hit = wParam;

            g_DragResizeState.lastHit = wParam;


            if (hit == HTCUSTOM_CLOSE) {
                g_DragResizeState.mouseDownClose = true;
            }
            else if (hit == HTCUSTOM_MAX) {
                g_DragResizeState.mouseDownMax = true;
            } 
            else if (hit == HTCUSTOM_RETORE){
                g_DragResizeState.mouseDownRestore = true;
            }
            else if ( hit == HTCUSTOM_MIN) {
                g_DragResizeState.mouseDownMin = true;
            }else{

            }
            
            
            break;
        }


        case WM_NCLBUTTONUP:
        {
            
            LRESULT hit = wParam;

            if(hit == g_DragResizeState. lastHit){
                if (hit == HTCUSTOM_CLOSE) {
                    SendMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
                    
                }
                else if (hit == HTCUSTOM_MAX) {
                    SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                    
                } 
                else if (hit == HTCUSTOM_RETORE){
                    SendMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                    
                }
                else if ( hit == HTCUSTOM_MIN) {
                    SendMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);  
                    
                }
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
        
        /*
        case WM_ENTERSIZEMOVE:
        {
            if (sdlWindow) {
                //SyncSDLWithWinAPI(sdlWindow);
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_ENTER);
            }
            g_DragResizeState.workArea = GetMonitorRectForWindow(hwnd);
            break;
        }
        case WM_EXITSIZEMOVE: {
            if (!g_DragResizeState.snapEnabled) break;
            if (sdlWindow) {
                //yncSDLWithWinAPI(sdlWindow);
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_LEAVE);
            }
            
            break;
        }

        // ===== Di chuyển =====
        case WM_MOVE:{
            break;
        }
        case WM_MOVING: {
            RECT rc; GetWindowRect(hwnd, &rc);
            if (sdlWindow) {
                //SyncSDLWithWinAPI(sdlWindow);
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_MOVED, rc.left, rc.top);
            }
            break;
        }

        
        // ===== Thay đổi kích thước / DPI =====
        case WM_SIZE: {
            if (!sdlWindow) break;
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
            //SyncSDLWithWinAPI(sdlWindow);
            int w = LOWORD(lParam), h = HIWORD(lParam);

            if (wParam == SIZE_MAXIMIZED) {
                //SDLX_SavePlacement(sdlWindow);
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_MAXIMIZED, w, h);
            } else if (wParam == SIZE_MINIMIZED) {
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_MINIMIZED, w, h);
            } else if (wParam == SIZE_RESTORED) {
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_RESTORED, w, h);
            }else{
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_RESIZED, w, h);
            }
            break;
        }
        case WM_SIZING: {
            RECT* rc = (RECT*)lParam;
            KeepAspectRatio(rc, (int)wParam);
            break;
        }
        */
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

        // ===== System commands / Timer =====
        /*
        case WM_SYSCOMMAND:
        {   
            switch (wParam & 0xFFF0) 
            {
            case SC_CLOSE:{
                //AnimateWindow(hwnd, 150, AW_BLEND | AW_HIDE);
                //g_animator->Animate(DWMAnimType::FadeOut, 150);
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_CLOSE, 0, 0);
                return  DefWindowProc( hwnd, msg, wParam, lParam);
            }
            case SC_MINIMIZE:{
                //g_animator->Animate(DWMAnimType::FadeOut, 150);
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
                return  DefWindowProc( hwnd, msg, wParam, lParam);
            }
            case SC_RESTORE:{
                //g_animator->Animate(DWMAnimType::FadeIn, 200);
                //SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_RESTORED, 0, 0);
                SyncSDLWithWinAPI(sdlWindow);
                return  DefWindowProc( hwnd, msg, wParam, lParam);
            }
            case SC_MAXIMIZE:{
               // g_animator->Animate(DWMAnimType::SlideIn, 180);
                //SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_MAXIMIZED, 0, 0);
                SyncSDLWithWinAPI(sdlWindow);
                return DefWindowProc( hwnd, msg, wParam, lParam);
                         
            }
            case SC_MOVE:      // Di chuyển cửa sổ
                return  DefWindowProc( hwnd, msg, wParam, lParam);
            case SC_SIZE:      // Resize cửa sổ
                return  DefWindowProc( hwnd, msg, wParam, lParam);
            }
            
            break;
            
        }
        case WM_TIMER:{

            //if (wParam == 1) {
            //    if (sdlWindow) {
            //        SyncSDLWithWinAPI(sdlWindow);
            //    }
            //    KillTimer(hwnd, 1);
            //}
            break;
        }
        // ===== Focus / Hiển thị =====
        case WM_SETFOCUS: {
            if (sdlWindow) 
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_FOCUS_GAINED); 
            break;
        }
        case WM_KILLFOCUS: {
            if (sdlWindow) 
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_FOCUS_LOST); 
            break;
        }
        case WM_SHOWWINDOW:{
            if (sdlWindow) {
                //SyncSDLWithWinAPI(sdlWindow);
                SDLX_PushEvent(sdlWindow, wParam ? SDL_WINDOWEVENT_SHOWN : SDL_WINDOWEVENT_HIDDEN);
            }
            break;
        }
        // ===== Khác =====
        case WM_DISPLAYCHANGE:
            if (sdlWindow) {
                //SyncSDLWithWinAPI(sdlWindow);
                SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_DISPLAY_CHANGED);
            }
            break;
        case WM_WINDOWPOSCHANGED:
            //g_DragResizeState.workArea = GetMonitorRectForWindow(hwnd);
            break;
        */
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
        //    if (sdlWindow) SDLX_PushClose(sdlWindow);
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
        //case WM_DESTROY:
            //if (sdlWindow) SDLX_PushClose(sdlWindow);
        //    break;
    }

    return CallWindowProc(g_DragResizeState.g_OldWndProc ?
                          g_DragResizeState.g_OldWndProc : DefWindowProc,
                          hwnd, msg, wParam, lParam);
}


void SDLX_InitBorderless(SDL_Window* window, int titleHeight, int resizeMargin)
{
    if (!window) return;
    
    // Lấy HWND
    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) return;
    HWND hwnd = wmInfo.info.win.window;

    g_DragResizeState.hwnd_windown_main = hwnd;
    g_DragResizeState.TitleHeight = titleHeight;
    g_DragResizeState.resizeMargin = resizeMargin;

    // Lưu SDL_Window để WndProc lấy lại
    SetProp(hwnd, L"SDLWIN", (HANDLE)window);
    
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style |= (  WS_MINIMIZEBOX  |
                  //WS_OVERLAPPED | 
                  WS_THICKFRAME  |
                    //WS_SYSMENU |
                    WS_CAPTION 
           //WS_OVERLAPPEDWINDOW
    );
    if (g_DragResizeState.snapEnabled )
    {
        style |= ( WS_MAXIMIZEBOX );
    }
    g_DragResizeState.style = style;
    SetWindowLong(hwnd, GWL_STYLE, style);
    
    /*LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);*/

    SetWindowPos(hwnd, NULL, 0,0,0,0,
                SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

    // Hook WndProc nếu chưa
    //SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)CustomWndProc);
    if (!g_DragResizeState.g_OldWndProc) {
        g_DragResizeState.g_OldWndProc = (WNDPROC)SetWindowLongPtr(
            hwnd, GWLP_WNDPROC,
            (LONG_PTR)+[](HWND h, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                SDL_Window* sdlWin = (SDL_Window*)GetProp(h, L"SDLWIN"); // Dùng h thay vì hwnd
                return CustomWndProc(h, msg, wParam, lParam, sdlWin);
            }
        );
    }
    // Cập nhật workArea & DPI ban đầu
    //g_DragResizeState.workArea = GetMonitorRectForWindow(hwnd);
    //UINT dpi = GetDpiForWindow ? GetDpiForWindow(hwnd) : 96;
    //g_DragResizeState.dpiX = dpi; g_DragResizeState.dpiY = dpi;

    //SDL_SetWindowBordered(window,SDL_FALSE);



}


// -------------------- UI: Titlebar + Control Buttons --------------------

// =================== Cách dùng ===================
// 1) Sau khi tạo SDL_Window* win:
//      HookSDLWindowProc(win);
// 2) Mỗi frame:
//      RenderBorderlessWindow(win, "Your Title", state);
//      SyncWinAPI_SDL(win, state);


