#pragma once

#include "windows/windows_borderless_state.h"
#include "globals.h"
#include "utils.h"
#include "imgui.h"

#include <SDL.h>

#include <windows.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <wrl.h>

/*#ifndef __IDCompositionVisual2_INTERFACE_DEFINED__
#define __IDCompositionVisual2_INTERFACE_DEFINED__

MIDL_INTERFACE("E8DE1639-4331-4B26-BC5F-6A321D347A85")
IDCompositionVisual2 : public IDCompositionVisual
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetOpacityAnimation(
        _In_opt_ IDCompositionAnimation *animation) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOffsetXAnimation(
        _In_opt_ IDCompositionAnimation *animation) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOffsetYAnimation(
        _In_opt_ IDCompositionAnimation *animation) = 0;
};

#endif
using namespace Microsoft::WRL;*/

// Core
void SDLX_ShutdownBorderless(SDL_Window* window);
void SDLX_SetMinMax(SDL_Window* window, int minW, int minH, int maxW, int maxH);
void SDLX_SetAspectRatio(SDL_Window* window, int num, int den);
void SDLX_ClearAspectRatio(SDL_Window* window);
void SDLX_EnableSnap(bool enabled, int thresholdPx);
void SyncSDLWithWinAPI(SDL_Window* sdlWin);
void SDLX_PushMoved(SDL_Window* win, int x, int y);
void SDLX_PushEvent(SDL_Window* win, Uint8 evt, int d1=0, int d2=0);
void SDLX_PushClose(SDL_Window* /*win*/);
void GetNormalRect(HWND hwnd, RECT& out);
// Fullscreen
bool SDLX_ToggleFullscreen(SDL_Window* window, bool enable);

// Restore
void SDLX_SavePlacement(SDL_Window* window);
bool SDLX_RestoreWindowSmart(SDL_Window* window, POINT cursor);

// UI
void RenderBorderlessWindow(SDL_Window* sdlWindow, const char* title, BorderlessWindowState& state);

void SetWindowSDL(SDL_Window* window,
                  int minW = 0, int minH = 0,
                  int maxW = 0, int maxH = 0,
                  int aspectNum = 0, int aspectDen = 0,
                  bool enableSnap = true, int snapThreshold = 24);

inline RECT GetMonitorRectForWindow(HWND hwnd) {
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (hMon) GetMonitorInfo(hMon, &mi);
    return mi.rcWork; // rcWork = vùng khả dụng (không tính taskbar)
}
inline ImVec2 AddVec2(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x+b.x, a.y+b.y); }
inline ImVec2 CenterIcon(const ImVec2& pos, const ImVec2& btnSize, float iconW, float iconH) {
    return ImVec2(pos.x + (btnSize.x - iconW) * 0.5f, pos.y + (btnSize.y - iconH) * 0.5f);
}
/*enum class DWMAnimType {
    FadeIn,
    FadeOut,
    SlideIn,
    SlideOut,
    Minimize,
    Restore
};

class DWMAnimator {
public:
    bool Init(HWND hwnd);
    void Animate(DWMAnimType type, float durationMs = 200.0f);

private:
    HWND hwnd{};
    Microsoft::WRL::ComPtr<IDCompositionDevice> device;
    Microsoft::WRL::ComPtr<IDCompositionTarget> target;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> visual;
};*/
