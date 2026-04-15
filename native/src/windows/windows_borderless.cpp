#define _WIN32_WINNT 0x0A00

#include "globals.h"
#include "utils.h"
#include "imgui.h"

#include "windows/windows_borderless_state.h"
#include "windows/windows_borderless.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <windowsx.h>
#include <algorithm> 

#include <chrono>

static DragResizeState& g_DragResizeState = GetDragResizeState();

void SDLUtils::SDLX_PushUniqueEvent(const SDL_Event& eventData) {
    SDL_Event existingEvent;
        
    // Kiểm tra xem trong hàng đợi đã có event cùng loại (type) chưa
    // Lưu ý: Nếu là SDL_USEREVENT, bạn có thể kiểm tra thêm cả trường 'code'
    if (SDL_PeepEvents(&existingEvent, 1, SDL_PEEKEVENT, eventData.type, eventData.type) == 0) {
        // Nếu chưa có, copy dữ liệu và đẩy vào
        SDL_Event eventToPush = eventData;
        SDL_PushEvent(&eventToPush);
    }
}

void SDLUtils::SDLX_SetMinMax(SDL_Window* window, int minW, int minH, int maxW, int maxH)
{
    if (!window) return;

    HWND hwnd = g_DragResizeState.hwnd_windown_main;

    RECT wrk = SDLUtils::GetMonitorRectForWindow(hwnd);
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
bool SDLUtils::SDLX_ToggleFullscreen(SDL_Window* window, bool enable)
{
    if (!window) return false;
    #ifdef CUSTOM_TITLEBAR
        if (!g_DragResizeState.hwnd_windown_main) {
            SDL_SysWMinfo wmInfo{};
            SDL_VERSION(&wmInfo.version);
            if (!SDL_GetWindowWMInfo(window, &wmInfo)) return false;
            g_DragResizeState.hwnd_windown_main = wmInfo.info.win.window;
        }
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
    #else
        Uint32 fullscreenFlag = SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (enable) {
            if (SDL_SetWindowFullscreen(window, fullscreenFlag) != 0) {
                SDL_Log("Failed to enter fullscreen: %s", SDL_GetError());
                return false;
            }
        } else {
            if (SDL_SetWindowFullscreen(window, 0) != 0) {
                SDL_Log("Failed to exit fullscreen: %s", SDL_GetError());
                return false;
            }
        }
        g_DragResizeState.IsFullscreen_video = enable;
        return g_DragResizeState.IsFullscreen_video;
    #endif
}
// -------------------- SDL event sync helpers --------------------
void SDLUtils::SyncSDLWithWinAPI(SDL_Window* sdlWin) {
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

void SDLUtils::SDLX_PushEvent(SDL_Window* win, Uint8 evt, int d1, int d2) {
    SDL_Event ev{};
    ev.type = SDL_WINDOWEVENT;
    ev.window.event = evt;
    ev.window.windowID = SDL_GetWindowID(win);
    ev.window.data1 = d1;
    ev.window.data2 = d2;
    SDL_PushEvent(&ev);
}
void SDLUtils::SDLX_PushClose(SDL_Window* /*win*/) {
    // Cho đa số app: SDL_WINDOWEVENT_CLOSE là chuẩn
    SDL_Event ev{};
    ev.type = SDL_WINDOWEVENT;
    ev.window.event = SDL_WINDOWEVENT_CLOSE;
    ev.window.windowID = 0; // hoặc SDL_GetWindowID(win) nếu cần phân biệt
    SDL_PushEvent(&ev);

    // Fallback chung:
    SDL_Event q{}; q.type = SDL_QUIT; SDL_PushEvent(&q);
}
RECT SDLUtils::GetMonitorRectForWindow(HWND hwnd) {
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (hMon) GetMonitorInfo(hMon, &mi);
    return mi.rcWork; // rcWork = vùng khả dụng (không tính taskbar)
}
void RenderBorderlessWindow(SDL_Window* sdlWindow, const char* title, DragResizeState& state ,ImVec2 winPos ,ImVec2 winSize) {
    
    #ifdef CUSTOM_TITLEBAR

    if (!sdlWindow || state.IsFullscreen_video) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 titlePos = winPos;
    ImVec2 titleSize(winSize.x, (float)state.TitleHeight);

    ImGui::SetCursorScreenPos(titlePos);
    ImGui::InvisibleButton("TitleBarRegion", titleSize);
    //bool hoveredTitle = ImGui::IsItemHovered();
    //ImU32 btnBgColor = hoveredTitle ? IM_COL32(60,60,60,255) : IM_COL32(35,35,35,255);
    ImU32 btnBgColor = IM_COL32(35,35,35,255);
    dl->AddRectFilled(titlePos, titlePos + titleSize, btnBgColor);

    dl->AddText(
        ImVec2(titlePos.x + 10.0f, titlePos.y + (state.TitleHeight - ImGui::GetTextLineHeight()) * 0.5f),
        IM_COL32(255,255,255,255),
        title
    );

    const ImVec2 btnSize(state.btnSize, state.TitleHeight ); // giữ nguyên chiều cao
    const float pad = 8.0f;

    // Close ❌
    ImVec2 closePos(titlePos.x + winSize.x - btnSize.x , titlePos.y );
    ImGui::SetCursorScreenPos(closePos);

    // Kiểm tra trạng thái hover / click
    bool clickedClose = ImGui::InvisibleButton("CloseBtn", btnSize);
    bool hoveredClose = ImGui::IsItemHovered();
    bool activeClose = (ImGui::IsItemHovered() && (state.mouseDownClose));

    // Điều chỉnh màu dựa vào trạng thái
    ImU32 closeColor = IM_COL32(35,35,35,255);       // mặc định đỏ vừa phải
    if (activeClose)  closeColor = IM_COL32(220,50,50,150);   // click đỏ rực rỡ
    else if (hoveredClose) closeColor = IM_COL32(220,50,50,255);  // hover đỏ tươi hơn
    
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
    
    ImVec2 maxPos(closePos.x - btnSize.x , titlePos.y );
    ImGui::SetCursorScreenPos(maxPos);

    bool clickedMax =ImGui::InvisibleButton("MaxRestoreBtn", btnSize);
    bool hoveredMax = ImGui::IsItemHovered();
    bool activeMax  = (ImGui::IsItemHovered() && (state.mouseDownMax || state.mouseDownRestore));


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
        if (!state.IsMax) {
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
    ImVec2 minPos(maxPos.x - btnSize.x , titlePos.y );
    ImGui::SetCursorScreenPos(minPos);
    
    bool clickedMin = ImGui::InvisibleButton("MinBtn", btnSize);
    if (clickedMin) {
        //ShowWindow(hwnd, SW_MINIMIZE);
        //SDLX_PushEvent(sdlWindow, SDL_WINDOWEVENT_MINIMIZED);
        //SDL_MinimizeWindow(sdlWindow);
    }
    bool hoveredMin = ImGui::IsItemHovered();
    bool activeMin  = (ImGui::IsItemHovered() && state.mouseDownMin);

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

    #endif
}
// Đăng ký hàm này ngay sau khi tạo window
void SDLUtils::SetWindowSDL(SDL_Window* window,
                  int minW, int minH,
                  int maxW, int maxH)
{
    if (!window) return;

    // 1. Hook WndProc + Init borderless
    SDLUtils::SDLX_InitBorderless(window);

    // 2. Thiết lập Min/Max size
    SDLUtils::SDLX_SetMinMax(window, minW, minH, maxW, maxH);

}

