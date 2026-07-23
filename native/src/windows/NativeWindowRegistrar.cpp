// NativeWindowRegistrar.cpp
#include "NativeWindowRegistrar.h"
#include "WindowRuntime.h"
#include <SDL_syswm.h>

// Extern WndProc
extern LRESULT CALLBACK MultiWindowWndProc(HWND, UINT, WPARAM, LPARAM);

void NativeWindowRegistrar::Register(WindowRuntime* runtime) {
    if (!runtime || !runtime->resource.sdlWindow) return;

    // Trích xuất HWND WinAPI và thực hiện Hook WndProc đa luồng / đa cửa sổ
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(runtime->resource.sdlWindow, &wmInfo);
    runtime->resource.hwnd = wmInfo.info.win.window;

    // Lưu con trỏ runtime vào HWND của WinAPI
    SetPropW(runtime->resource.hwnd, L"WINDOW_RUNTIME_PTR", (HANDLE)runtime);

    // Thực hiện gài đè WndProc
    WNDPROC oldProc = (WNDPROC)SetWindowLongPtrW(runtime->resource.hwnd, GWLP_WNDPROC, (LONG_PTR)MultiWindowWndProc);
    runtime->properties.Set<WNDPROC>("OldWndProc", oldProc);

    // Áp dụng các Style nâng cao của WinAPI
    LONG winStyle = GetWindowLong(runtime->resource.hwnd, GWL_STYLE);
    winStyle |= (WS_MINIMIZEBOX | WS_THICKFRAME | WS_CAPTION);
    if (runtime->style.snapEnabled) winStyle |= WS_MAXIMIZEBOX;
    runtime->style.Style = winStyle;
    SetWindowLong(runtime->resource.hwnd, GWL_STYLE, winStyle);
    SetWindowPos(runtime->resource.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}