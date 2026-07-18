
#include "WindowRuntime.h"
#include "MainWindowState.h"
void UpdateWindowState(WindowRuntime* runtime) {
    if (!runtime || !runtime->sdlWindow) return;

    // 1. Lấy và cập nhật các cờ trạng thái vật lý của SDL Window
    Uint32 flags = SDL_GetWindowFlags(runtime->sdlWindow);
    
    runtime->state.isShown = (flags & SDL_WINDOW_SHOWN) != 0;
    runtime->state.isMinimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    runtime->state.isMaximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    runtime->state.isFullscreen = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    
    // Cập nhật trạng thái hiển thị logic (Giống g_WindowVisible cũ)
    runtime->state.isVisible = runtime->state.isShown && !runtime->state.isMinimized;

    // 2. Cập nhật Tọa độ & Kích thước cửa sổ
    MainWindowLayout layout = runtime->properties.Get<MainWindowLayout>("Layout");
    SDL_GetWindowPosition(runtime->sdlWindow, &layout.WinX, &layout.WinY);
    SDL_GetWindowSize(runtime->sdlWindow, &layout.WinW, &layout.WinH);

    // 3. Tính toán hình học Layout dựa trên trạng thái (Ví dụ: Ẩn/Hiện Titlebar)
    if (runtime->state.isFullscreen) {
        layout.VideoPos = ImVec2(0, 0);
        layout.VideoSize = ImVec2(static_cast<float>(layout.WinW), static_cast<float>(layout.WinH));
        layout.TitleSize = ImVec2(0, 0);
    } else {
        float titleH = static_cast<float>(runtime->style.titleHeight);
        layout.TitlePos = ImVec2(static_cast<float>(layout.WinX), static_cast<float>(layout.WinY));
        layout.TitleSize = ImVec2(static_cast<float>(layout.WinW), titleH);

        layout.VideoPos = ImVec2(static_cast<float>(layout.WinX), static_cast<float>(layout.WinY) + titleH);
        layout.VideoSize = ImVec2(static_cast<float>(layout.WinW), static_cast<float>(layout.WinH) - titleH);
    }

    // Đồng bộ vùng tương tác chuột (SDL_Rect) cho Video Area[cite: 5]
    layout.videoArea.x = static_cast<int>(layout.VideoPos.x);
    layout.videoArea.y = static_cast<int>(layout.VideoPos.y);
    layout.videoArea.w = static_cast<int>(layout.VideoSize.x);
    layout.videoArea.h = static_cast<int>(layout.VideoSize.y);

    
    // 4. Đẩy ngược dữ liệu đã tính toán vào PropertyBag của Cửa sổ[cite: 5]
    runtime->properties.Set<MainWindowLayout>("Layout", layout);
}