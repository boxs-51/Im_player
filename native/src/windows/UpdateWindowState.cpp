// UpdateWindowState.cpp
#include "WindowRuntime.h"
#include "MainWindowState.h"
#include "mpv_render_video.h"
#include <SDL.h>

#ifdef RENDER_MPV_THREAD
#include "globals.h" // Chứa thông tin biến renderThread toàn cục của bạn
#endif


// Hàm 1: Cập nhật trạng thái vật lý chung cho TẤT CẢ các cửa sổ
void UpdateWindowStateCommon(WindowRuntime* runtime) {
    if (!runtime || !runtime->sdlWindow) return;

    //Uint32 flags = SDL_GetWindowFlags(runtime->sdlWindow);
    //runtime->state.isShown = (flags & SDL_WINDOW_SHOWN) != 0;
    //runtime->state.isMinimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    //runtime->state.isMaximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    //runtime->state.isFullscreen = ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) || 
    //                          ((flags & SDL_WINDOW_FULLSCREEN) != 0);
    
    runtime->state.isVisible = runtime->state.isShown && !runtime->state.isMinimized;
}

// Hàm 2: Cập nhật chuyên biệt cho Window Master (Tính toán hình học phức tạp + Đẩy kích thước sang MPV)
void UpdateMainWindowState(WindowRuntime* runtime) {
    if (!runtime || !runtime->sdlWindow) return;

    // Bước 1: Đồng bộ các cờ vật lý cơ bản trước
    UpdateWindowStateCommon(runtime);

    // Bước 2: Tính toán Hình học Layout dựa trên PropertyBag
    MainWindowLayout layout = runtime->properties.Get<MainWindowLayout>("Layout");
    SDL_GetWindowPosition(runtime->sdlWindow, &layout.WinX, &layout.WinY);
    SDL_GetWindowSize(runtime->sdlWindow, &layout.WinW, &layout.WinH);

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

    // Đẩy ngược dữ liệu đã tính toán vào PropertyBag[cite: 5]
    runtime->properties.Set<MainWindowLayout>("Layout", layout);

    // Bước 3: ĐỒNG BỘ TRỰC TIẾP SANG CHO LUỒNG MPV (Chỉ WindowMaster chịu trách nhiệm)
#ifdef RENDER_MPV_THREAD
    renderThread.g_WindowVisible.store(runtime->state.isVisible, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(renderThread.mtx);
    if (renderThread.surface.drawW != (int)layout.VideoSize.x || 
        renderThread.surface.drawH != (int)layout.VideoSize.y) {
        
        renderThread.surface.newW = (int)layout.VideoSize.x;
        renderThread.surface.newH = (int)layout.VideoSize.y;
        renderThread.surface.needResize = true;
    }
#endif
}