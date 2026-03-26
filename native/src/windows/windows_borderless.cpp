#define _WIN32_WINNT 0x0A00
#define UNICODE
#include <SDL.h>
#include "globals.h"
#include "utils.h"
#include "imgui.h"

#include "windows/windows_borderless_state.h"
#include "windows/windows_borderless.h"
#include "windows/windows_custom_titlebar.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <windowsx.h>
#include <algorithm> 

#include <chrono>




void SDLX_ShutdownBorderless(SDL_Window* window)
{
    if (!window) return;
    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) return;
    HWND hwnd = wmInfo.info.win.window;

    if (g_DragResizeState.g_OldWndProc) {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)g_DragResizeState.g_OldWndProc);
        g_DragResizeState.g_OldWndProc = nullptr;
    }
}
void SDLX_SetMinMax(SDL_Window* window, int minW, int minH, int maxW, int maxH)
{
    if (!window) return;

    HWND hwnd = g_DragResizeState.hwnd_windown_main;

    RECT wrk = GetMonitorRectForWindow(hwnd);
    int screenW = wrk.right - wrk.left;
    int screenH = wrk.bottom - wrk.top;

    g_DragResizeState.sdlMinW = (minW > 0 && minW <= screenW) ? minW : 0;
    g_DragResizeState.sdlMinH = (minH > 0 && minH <= screenH) ? minH : 0;
    g_DragResizeState.sdlMaxW = (maxW > 0 && maxW <= screenW) ? maxW : 0;
    g_DragResizeState.sdlMaxH = (maxH > 0 && maxH <= screenH) ? maxH : 0;

    if (g_DragResizeState.sdlMinW > 0 && g_DragResizeState.sdlMinH > 0)
        SDL_SetWindowMinimumSize(window, g_DragResizeState.sdlMinW, g_DragResizeState.sdlMinH);
    if (g_DragResizeState.sdlMaxW > 0 && g_DragResizeState.sdlMaxH > 0)
        SDL_SetWindowMaximumSize(window, g_DragResizeState.sdlMaxW, g_DragResizeState.sdlMaxH);
}
void SDLX_SetAspectRatio(SDL_Window* /*window*/, int num, int den)
{
    if (num <= 0 || den <= 0) { SDLX_ClearAspectRatio(nullptr); return; }
    g_DragResizeState.aspectNum = num;
    g_DragResizeState.aspectDen = den;
    g_DragResizeState.aspectLock = true;
}
void SDLX_ClearAspectRatio(SDL_Window* /*window*/)
{
    g_DragResizeState.aspectLock = false;
    g_DragResizeState.aspectNum = 0;
    g_DragResizeState.aspectDen = 0;
}
void SDLX_EnableSnap(bool enabled, int thresholdPx)
{
    g_DragResizeState.snapEnabled = enabled;
    if (thresholdPx > 0) g_DragResizeState.snapThreshold = thresholdPx;
}
bool SDLX_ToggleFullscreen(SDL_Window* window, bool enable)
{
    if (!window) return false;

    HWND hwnd = g_DragResizeState.hwnd_windown_main;

    if (!g_DragResizeState.IsFullscreen_video)
    {
        // Lưu trạng thái windowed trước fullscreen
        GetWindowRect(hwnd, &g_DragResizeState.fullscreenRestoreRect);
        g_DragResizeState.placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd, &g_DragResizeState.placement);

        SDL_GetWindowSize(window, &g_DragResizeState.restoreW, &g_DragResizeState.restoreH);

        // Phủ toàn màn hình desktop
        RECT rcScreen;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0);
        SetWindowPos(hwnd, HWND_TOP, rcScreen.left, rcScreen.top,
                     rcScreen.right - rcScreen.left,
                     rcScreen.bottom - rcScreen.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        g_DragResizeState.IsFullscreen_video = true;
    }
    else if (g_DragResizeState.IsFullscreen_video)
    {

        // Restore vị trí và kích thước windowed
        RECT rc = g_DragResizeState.fullscreenRestoreRect;
        SetWindowPos(hwnd, HWND_TOP,
                     rc.left, rc.top,
                     rc.right - rc.left,
                     rc.bottom - rc.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        // Restore trạng thái maximize/minimize
        SetWindowPlacement(hwnd, &g_DragResizeState.placement);

        // Đồng bộ lại kích thước SDL
        SDL_SetWindowPosition(window, rc.left, rc.top);
        SDL_SetWindowSize(window, g_DragResizeState.restoreW, g_DragResizeState.restoreH);

        g_DragResizeState.IsFullscreen_video = false;
    }

    return g_DragResizeState.IsFullscreen_video;
}

void SDLX_SavePlacement(SDL_Window* window)
{
    if (!window) return;
    SDL_SysWMinfo wmInfo{}; SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) return;
    HWND hwnd = wmInfo.info.win.window;

    GetNormalRect(hwnd, g_DragResizeState.restoreRect);
}
bool SDLX_RestoreWindowSmart(SDL_Window* window, POINT cursor)
{
    if (!window) return false;

    SDL_SysWMinfo wmInfo{}; SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) return false;
    HWND hwnd = wmInfo.info.win.window;

    RECT rr{};
    int rw, rh;


    rr = g_DragResizeState.restoreRect;
    rw = rr.right - rr.left;
    rh = rr.bottom - rr.top;

    // --- Tính lại vị trí: con trỏ nằm giữa titlebar của cửa sổ restore ---
    int offsetX = rw / 2; // con trỏ ở giữa chiều ngang
    int offsetY = g_DragResizeState.TitleHeight / 2;

    int newX = cursor.x - offsetX;
    int newY = cursor.y - offsetY;

    SetWindowPos(hwnd, NULL, newX, newY, rw, rh, SWP_SHOWWINDOW);
    SDL_SetWindowPosition(window, newX, newY);
    SDL_SetWindowSize(window, rw, rh);


    // Reset state
    g_DragResizeState.snapState = SnapState::NONE;

    // Đồng bộ + event
    SyncSDLWithWinAPI(window);
    SDLX_PushEvent(window, SDL_WINDOWEVENT_RESTORED);

    int finalW = rr.right - rr.left;
    int finalH = rr.bottom - rr.top;
    SDLX_PushEvent(window, SDL_WINDOWEVENT_RESIZED, finalW, finalH);

    return true;
}

// -------------------- SDL event sync helpers --------------------
void SyncSDLWithWinAPI(SDL_Window* sdlWin) {
    if (!sdlWin || !g_DragResizeState.hwnd_windown_main) return;
    HWND hwnd = g_DragResizeState.hwnd_windown_main;

    RECT rcClient , rcWin; 
    GetClientRect(hwnd, &rcClient);
    GetWindowRect(hwnd, &rcWin);
    // toạ độ client -> screen, để SDL đặt đúng vị trí cửa sổ
    POINT pt = { rcClient.left, rcClient.top };
    ClientToScreen(hwnd, &pt);
    int x = pt.x;
    int y = pt.y;
    int w = rcClient.right - rcClient.left;
    int h = rcClient.bottom - rcClient.top;

    int curX, curY, curW, curH;
    SDL_GetWindowPosition(sdlWin, &curX, &curY);
    SDL_GetWindowSize(sdlWin, &curW, &curH);

    if (curX != x || curY != y) SDL_SetWindowPosition(sdlWin, x, y);
    if (curW != w || curH != h) SDL_SetWindowSize(sdlWin, w, h);
}
void SDLX_PushMoved(SDL_Window* win, int x, int y) {
    SDL_Event ev{};
    ev.type = SDL_WINDOWEVENT;
    ev.window.event = SDL_WINDOWEVENT_MOVED;
    ev.window.windowID = SDL_GetWindowID(win);
    ev.window.data1 = x;
    ev.window.data2 = y;
    SDL_PushEvent(&ev);
}
void SDLX_PushEvent(SDL_Window* win, Uint8 evt, int d1, int d2) {
    SDL_Event ev{};
    ev.type = SDL_WINDOWEVENT;
    ev.window.event = evt;
    ev.window.windowID = SDL_GetWindowID(win);
    ev.window.data1 = d1;
    ev.window.data2 = d2;
    SDL_PushEvent(&ev);
}
void SDLX_PushClose(SDL_Window* /*win*/) {
    // Cho đa số app: SDL_WINDOWEVENT_CLOSE là chuẩn
    SDL_Event ev{};
    ev.type = SDL_WINDOWEVENT;
    ev.window.event = SDL_WINDOWEVENT_CLOSE;
    ev.window.windowID = 0; // hoặc SDL_GetWindowID(win) nếu cần phân biệt
    SDL_PushEvent(&ev);

    // Fallback chung:
    SDL_Event q{}; q.type = SDL_QUIT; SDL_PushEvent(&q);
}

void GetNormalRect(HWND hwnd, RECT& out)
{
    WINDOWPLACEMENT wp{ sizeof(WINDOWPLACEMENT) };
    if (GetWindowPlacement(hwnd, &wp) && wp.showCmd != SW_SHOWMAXIMIZED && wp.showCmd != SW_SHOWMINIMIZED) {
        out = wp.rcNormalPosition;
    } else {
        GetWindowRect(hwnd, &out);
    }
}
void RenderBorderlessWindow(SDL_Window* sdlWindow, const char* title, BorderlessWindowState& state) {
    if (!sdlWindow || state.isFullscreen_video) return;
    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(sdlWindow, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;
    g_DragResizeState.hwnd_windown_main = hwnd;

    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 titlePos = winPos;
    ImVec2 titleSize(winSize.x, (float)state.titleHeight);

    ImGui::SetCursorScreenPos(titlePos);
    ImGui::InvisibleButton("TitleBarRegion", titleSize);
    //bool hoveredTitle = ImGui::IsItemHovered();
    //ImU32 btnBgColor = hoveredTitle ? IM_COL32(60,60,60,255) : IM_COL32(35,35,35,255);
    ImU32 btnBgColor = /*hoveredTitle ? IM_COL32(60,60,60,255) : */IM_COL32(35,35,35,255);
    dl->AddRectFilled(titlePos, titlePos + titleSize, btnBgColor);

    dl->AddText(
        ImVec2(titlePos.x + 10.0f, titlePos.y + (state.titleHeight - ImGui::GetTextLineHeight()) * 0.5f),
        IM_COL32(255,255,255,255),
        title
    );

    const ImVec2 btnSize(30, state.titleHeight ); // giữ nguyên chiều cao
    const float pad = 8.0f, gap = 0.0f;

    // Close ❌
    ImVec2 closePos(titlePos.x + winSize.x - btnSize.x - gap, titlePos.y );
    ImGui::SetCursorScreenPos(closePos);

    // Kiểm tra trạng thái hover / click
    bool clickedClose = ImGui::InvisibleButton("CloseBtn", btnSize);
    bool hoveredClose = ImGui::IsItemHovered();
    bool activeClose = (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left));

    if (clickedClose) SDLX_PushClose(sdlWindow);
    // Điều chỉnh màu dựa vào trạng thái
    //ImU32 closeColor = IM_COL32(35,35,35,255);       // mặc định
    ImU32 closeColor = IM_COL32(35,35,35,255);       // mặc định đỏ vừa phải
    if (hoveredClose) closeColor = IM_COL32(220,50,50,255);  // hover đỏ tươi hơn
    if (activeClose)  closeColor = IM_COL32(180,40,40,255);   // click đỏ rực rỡ

    dl->AddRectFilled(closePos, closePos + btnSize, closeColor, 4.0f);
    {
        // ----- Xác định center -----
        float iconPad   = 7.0f;   
        float lineThick = 2.0f;
        float lineScale = 1.0f;
        float linesize1 = 30.0f;
        float linesize2 = 30.0f;

        // Giao điểm trung tâm của X
        ImVec2 center = ImVec2(closePos.x + btnSize.x * 0.5f, closePos.y + btnSize.y * 0.5f);

        // Tùy chỉnh góc từng thanh tại giao điểm
        float angle1 = 45.0f;   // góc đường chéo 1 (độ) so với trục ngang
        float angle2 = -45.0f;  // góc đường chéo 2 (độ) so với trục ngang

        // Tùy chỉnh chiều dài từng thanh
        float halfLen1 = (linesize1 * 0.5f - iconPad) * lineScale;
        float halfLen2 = (linesize2 * 0.5f - iconPad) * lineScale;

        // Hàm chuyển độ sang rad
        auto Deg2Rad = [](float deg){ return deg * 3.14159265f / 180.0f; };

        // Vector đường chéo 1
        ImVec2 dir1 = ImVec2(cosf(Deg2Rad(angle1)) * halfLen1,
                            sinf(Deg2Rad(angle1)) * halfLen1);

        // Vector đường chéo 2
        ImVec2 dir2 = ImVec2(cosf(Deg2Rad(angle2)) * halfLen2,
                            sinf(Deg2Rad(angle2)) * halfLen2);

        // Tính điểm đầu cuối từng đường chéo dựa vào center
        ImVec2 c1 = center - dir1;
        ImVec2 c2 = center + dir1;

        ImVec2 c3 = center - dir2;
        ImVec2 c4 = center + dir2;

        // Vẽ 2 đường chéo
        ImU32 color = IM_COL32(255,255,255,255);
        dl->AddLine(c1, c2, color, lineThick);
        dl->AddLine(c3, c4, color, lineThick);
    }

    // Maximize 🗖 / Restore 🗗
    g_DragResizeState.IsMax = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_MAXIMIZED) != 0;
    ImVec2 maxPos(closePos.x - btnSize.x - gap, titlePos.y );
    ImGui::SetCursorScreenPos(maxPos);

    if (ImGui::InvisibleButton("MaxRestoreBtn", btnSize)) {
        if (!g_DragResizeState.IsMax) {
            ShowWindow(hwnd, SW_MAXIMIZE);
            RECT wr{}; GetWindowRect(hwnd, &wr);
            SDL_SetWindowPosition(sdlWindow, wr.left, wr.top);
            SDL_SetWindowSize(sdlWindow, wr.right - wr.left, wr.bottom - wr.top);
            SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_MAXIMIZED);
        } else {
            ShowWindow(hwnd, SW_RESTORE);
            RECT wr{}; GetWindowRect(hwnd, &wr);
            SDL_SetWindowPosition(sdlWindow, wr.left, wr.top);
            SDL_SetWindowSize(sdlWindow, wr.right - wr.left, wr.bottom - wr.top);
            SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_RESTORED);
        }
    }

    bool hoveredMax = ImGui::IsItemHovered();
    bool activeMax  = (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left));


    ImU32 maxColor = IM_COL32(35,35,35,255);
    if (hoveredMax) maxColor = IM_COL32(60,60,60,255);
    if (activeMax)  maxColor = IM_COL32(90,90,90,255);
    dl->AddRectFilled(maxPos, maxPos + btnSize, maxColor, 4.0f);
    {
        float iconPad     = 7.0f;    // khoảng cách từ viền nút đến icon
        float iconBorder  = 0.0f;    // độ dày viền (stroke)
        float iconRounding = 1.0f;   // bán kính bo góc
        float iconScaleY  = 0.8f;    // tỉ lệ chiều cao
        float iconScaleX  = 0.8f;    // tỉ lệ chiều rộng
        if (!g_DragResizeState.IsMax) {
            float iconScaleY  = 1.0f;    // tỉ lệ chiều cao
            ImVec2 iconPos = ImVec2(maxPos.x + iconPad, maxPos.y + iconPad);
            ImVec2 iconSize = ImVec2(
                (btnSize.x - 2 * iconPad) * iconScaleX,
                (btnSize.y - 2 * iconPad) * iconScaleY
            );
            dl->AddRect(iconPos, iconPos + iconSize, IM_COL32(255,255,255,255), iconRounding, 0, iconBorder);
        } else {
            ImVec2 backPos  = ImVec2(maxPos.x + iconPad + 2, maxPos.y + iconPad + 2);
            ImVec2 frontPos = ImVec2(maxPos.x + iconPad,     maxPos.y + iconPad);
            ImVec2 iconSize = ImVec2(
                (btnSize.x - 2 * iconPad - 2) * iconScaleX,
                (btnSize.y - 2 * iconPad - 2) * (iconScaleY + 0.4f)
            );
            dl->AddRect(backPos,  backPos  + iconSize, IM_COL32(255,255,255,255), iconRounding, 0, iconBorder);
            dl->AddRect(frontPos, frontPos + iconSize, IM_COL32(255,255,255,255), iconRounding, 0, iconBorder);
        }
    }

    // Minimize ➖
    ImVec2 minPos(maxPos.x - btnSize.x - gap, titlePos.y );
    ImGui::SetCursorScreenPos(minPos);
    
    if (ImGui::InvisibleButton("MinBtn", btnSize)) {
        ShowWindow(hwnd, SW_MINIMIZE);
        SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_MINIMIZED);
        SDL_MinimizeWindow(sdlWindow);
    }
    bool hoveredMin = ImGui::IsItemHovered();
    bool activeMin  = (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left));

    ImU32 minColor = IM_COL32(35,35,35,255);
    if (hoveredMin) minColor = IM_COL32(60,60,60,255);
    if (activeMin)  minColor = IM_COL32(90,90,90,255);
    dl->AddRectFilled(minPos, minPos + btnSize, minColor, 4.0f);
    {
        float linesize = 30.f;
        ImVec2 lineStart = ImVec2(minPos.x + pad, minPos.y + btnSize.y / 2);
        ImVec2 lineEnd   = ImVec2(minPos.x + linesize - pad, minPos.y + btnSize.y / 2);
        dl->AddLine(lineStart, lineEnd, IM_COL32(255,255,255,255), 2.0f);
    }

    state.videoOffset.y = state.titleHeight;
    float totalControlWidth = 3 * btnSize.x + 2 * gap;
    state.controlWidth = winSize.x - totalControlWidth - gap;
    g_DragResizeState.ControlWidth = state.controlWidth;

    auto ToRECT = [hwnd](ImVec2 pos, ImVec2 size) {
        RECT r{};
        r.left   = (LONG)pos.x;
        r.top    = (LONG)pos.y;
        r.right  = (LONG)(pos.x + size.x);
        r.bottom = (LONG)(pos.y + size.y);
        POINT tl = { r.left, r.top };
        POINT br = { r.right, r.bottom };
        ScreenToClient(hwnd, &tl);
        ScreenToClient(hwnd, &br);
        r.left   = tl.x; r.top    = tl.y;
        r.right  = br.x; r.bottom = br.y;
        return r;
    };

    g_DragResizeState.rcClose = ToRECT(closePos, btnSize);
    g_DragResizeState.rcMax   = ToRECT(maxPos, btnSize);
    g_DragResizeState.rcMin   = ToRECT(minPos, btnSize);

    SyncSDLWithWinAPI(sdlWindow);
}

void SetWindowSDL(SDL_Window* window,
                  int minW, int minH,
                  int maxW, int maxH,
                  int aspectNum , int aspectDen,
                  bool enableSnap , int snapThreshold )
{
    if (!window) return;

    // 1. Hook WndProc + Init borderless
    SDLX_InitBorderless(window, BW.titleHeight, BW.resizeMargin);

    // 2. Thiết lập Min/Max size
    SDLX_SetMinMax(window, minW, minH, maxW, maxH);

    // 3. Aspect ratio (nếu có)
    if (aspectNum > 0 && aspectDen > 0)
        SDLX_SetAspectRatio(window, aspectNum, aspectDen);

    // 4. Snap-to-edge
    SDLX_EnableSnap(enableSnap, snapThreshold);
}/*
bool DWMAnimator::Init(HWND hwnd) {
    this->hwnd = hwnd;

    HRESULT hr = DCompositionCreateDevice(nullptr, __uuidof(IDCompositionDevice),
                                          (void**)&device);
    if (FAILED(hr)) return false;

    hr = device->CreateTargetForHwnd(hwnd, TRUE, &target);
    if (FAILED(hr)) return false;

    hr = device->CreateVisual(&visual);
    if (FAILED(hr)) return false;

    hr = target->SetRoot(visual.Get());
    return SUCCEEDED(hr);
}

void DWMAnimator::Animate(DWMAnimType type, float durationMs)
{
    if (!visual || !device) return;

    ComPtr<IDCompositionAnimation> animX;
    ComPtr<IDCompositionAnimation> animY;
    ComPtr<IDCompositionAnimation> animOpacity;

    device->CreateAnimation(&animOpacity);
    device->CreateAnimation(&animX);
    device->CreateAnimation(&animY);

    const float durationSec = durationMs / 1000.0f;

    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;  QueryPerformanceCounter(&now);
    double startTime = static_cast<double>(now.QuadPart) / freq.QuadPart;

    switch (type)
    {
    case DWMAnimType::FadeIn:
        animOpacity->AddCubic(startTime, 0.0f, 0.0f, 1.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::FadeOut:
        animOpacity->AddCubic(startTime, 1.0f, 0.0f, 0.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::SlideIn:
        animY->AddCubic(startTime, 100.0f, 0.0f, 0.0f, durationSec);
        visual->SetOffsetYAnimation(animY.Get());
        animOpacity->AddCubic(startTime, 0.0f, 0.0f, 1.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::SlideOut:
        animY->AddCubic(startTime, 0.0f, 0.0f, 100.0f, durationSec);
        visual->SetOffsetYAnimation(animY.Get());
        animOpacity->AddCubic(startTime, 1.0f, 0.0f, 0.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::Minimize:
        animOpacity->AddCubic(startTime, 1.0f, 0.0f, 0.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::Restore:
        animOpacity->AddCubic(startTime, 0.0f, 0.0f, 1.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;
    }

    device->Commit();
}
bool DWMAnimator::Init(HWND hwnd) {
    this->hwnd = hwnd;

    HRESULT hr = DCompositionCreateDevice(nullptr, __uuidof(IDCompositionDevice),
                                          (void**)&device);
    if (FAILED(hr)) return false;

    hr = device->CreateTargetForHwnd(hwnd, TRUE, &target);
    if (FAILED(hr)) return false;

    hr = device.As(&visual); // chuyển sang IDCompositionVisual2
    if (FAILED(hr)) {
        ComPtr<IDCompositionVisual> visualBase;
        device->CreateVisual(&visualBase);
        visualBase.As(&visual);
    }

    if (!visual) return false;

    hr = target->SetRoot(visual.Get());
    return SUCCEEDED(hr);
}
void DWMAnimator::Animate(DWMAnimType type, float durationMs) {
    if (!visual || !device) return;

    ComPtr<IDCompositionAnimation> animX;
    ComPtr<IDCompositionAnimation> animY;
    ComPtr<IDCompositionAnimation> animOpacity;

    device->CreateAnimation(&animOpacity);
    device->CreateAnimation(&animX);
    device->CreateAnimation(&animY);

    const float durationSec = durationMs / 1000.0f;

    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;  QueryPerformanceCounter(&now);
    double startTime = static_cast<double>(now.QuadPart) / freq.QuadPart;

    switch (type)
    {
    case DWMAnimType::FadeIn:
        animOpacity->AddCubic(startTime, 0.0f, 0.0f, 1.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::FadeOut:
        animOpacity->AddCubic(startTime, 1.0f, 0.0f, 0.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::SlideIn:
        animY->AddCubic(startTime, 100.0f, 0.0f, 0.0f, durationSec);
        visual->SetOffsetYAnimation(animY.Get());
        animOpacity->AddCubic(startTime, 0.0f, 0.0f, 1.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::SlideOut:
        animY->AddCubic(startTime, 0.0f, 0.0f, 100.0f, durationSec);
        visual->SetOffsetYAnimation(animY.Get());
        animOpacity->AddCubic(startTime, 1.0f, 0.0f, 0.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::Minimize:
        animOpacity->AddCubic(startTime, 1.0f, 0.0f, 0.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;

    case DWMAnimType::Restore:
        animOpacity->AddCubic(startTime, 0.0f, 0.0f, 1.0f, durationSec);
        visual->SetOpacityAnimation(animOpacity.Get());
        break;
    }

    device->Commit();
}*/