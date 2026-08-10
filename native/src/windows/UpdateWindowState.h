// UpdateWindowState.cpp
#pragma once
#include "WindowRuntime.h"
#include "MainWindowState.h"
#include "WindowManager.h"
#include "WindowResource.h"
#include "UIRenderThread.h"

#include "MPVSession.h"
#include "mpv/mpv_data.h"
#include <SDL.h>

#ifdef RENDER_MPV_THREAD
#include "globals.h" // Chứa thông tin biến renderThread toàn cục của bạn
#endif


// Hàm 1: Cập nhật trạng thái vật lý chung cho TẤT CẢ các cửa sổ
inline void UpdateWindowStateCommon(WindowRuntime* runtime) {
    if (!runtime || !runtime->resource.sdlWindow) return;

    //Uint32 flags = SDL_GetWindowFlags(runtime->sdlWindow);
    //runtime->state.isShown = (flags & SDL_WINDOW_SHOWN) != 0;
    //runtime->state.isMinimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    //runtime->state.isMaximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    //runtime->state.isFullscreen = ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) || 
    //                          ((flags & SDL_WINDOW_FULLSCREEN) != 0);
    
    runtime->state.display.isVisible = runtime->state.display.isShown && !runtime->state.display.isMinimized;
}

// Hàm 2: Cập nhật chuyên biệt cho Window Master (Tính toán hình học phức tạp + Đẩy kích thước sang MPV)
inline bool UpdateWindowState(WindowRuntime* runtime) {
    if (!runtime || !runtime->resource.sdlWindow) return false;

    // Bước 1: Đồng bộ các cờ vật lý cơ bản trước
    UpdateWindowStateCommon(runtime);

    // Tạo các biến cục bộ để xử lý và so sánh
    WindowLayout localLayout;
    bool hasSizeChanged = false;

    // Lấy kích thước và vị trí từ SDL
    SDL_GetWindowPosition(runtime->resource.sdlWindow, &localLayout.WinX, &localLayout.WinY);
    SDL_GetWindowSize(runtime->resource.sdlWindow, &localLayout.WinW, &localLayout.WinH);

    bool use_viewpoint = false;
    if (!use_viewpoint) {
        localLayout.WinX = 0;
        localLayout.WinY = 0;
    }

    // Bước 2: Tính toán Hình học Layout trên biến cục bộ
    if (runtime->state.display.isFullscreen) {
        localLayout.ClientPos = ImVec2(0.0f, 0.0f);
        localLayout.ClientSize = ImVec2(static_cast<float>(localLayout.WinW), static_cast<float>(localLayout.WinH));
        localLayout.TitleSize = ImVec2(0.0f, 0.0f);
    } else {
        float titleH = static_cast<float>(runtime->style.titleHeight);
        localLayout.TitlePos = ImVec2(static_cast<float>(localLayout.WinX), static_cast<float>(localLayout.WinY));
        localLayout.TitleSize = ImVec2(static_cast<float>(localLayout.WinW), titleH);

        localLayout.ClientPos = ImVec2(static_cast<float>(localLayout.WinX), static_cast<float>(localLayout.WinY) + titleH);
        localLayout.ClientSize = ImVec2(static_cast<float>(localLayout.WinW), static_cast<float>(localLayout.WinH) - titleH);
    }

    // Đồng bộ vùng tương tác chuột (SDL_Rect)
    localLayout.ClientArea.x = static_cast<int>(localLayout.ClientPos.x);
    localLayout.ClientArea.y = static_cast<int>(localLayout.ClientPos.y);
    localLayout.ClientArea.w = static_cast<int>(localLayout.ClientSize.x);
    localLayout.ClientArea.h = static_cast<int>(localLayout.ClientSize.y);

    // Kiểm tra sự thay đổi kích thước dựa trên Layout cũ (nếu có)
    const auto* oldLayout = runtime->properties.GetPtr<WindowLayout>("Layout");
    if (!oldLayout || oldLayout->ClientSize.x != localLayout.ClientSize.x || oldLayout->ClientSize.y != localLayout.ClientSize.y) {
        hasSizeChanged = true;
        // Yêu cầu luồng UI thay đổi kích thước viewport đồ họa một cách an toàn
        if (runtime->resource.uiRenderThread) {
            runtime->resource.uiRenderThread->RequestResize(localLayout.ClientArea.w, localLayout.ClientArea.h);
        }
    }

    // Bước 3: ĐỒNG BỘ SANG LUỒNG MPV
    #ifdef RENDER_MPV_THREAD
    if (runtime->resource.mpvSession && runtime->style.isMainWindow) {
        std::weak_ptr<MPVRenderThread> weakRenderThread = runtime->resource.mpvSession->GetRenderThread();
        
        if (auto sharedRenderThread = weakRenderThread.lock()) {
            auto& renderState = sharedRenderThread->state;
            renderState.g_WindowVisible.store(runtime->state.display.isVisible, std::memory_order_relaxed);

            bool needsNotify = false;
            {
                std::lock_guard<std::mutex> lock(renderState.mtx);
                if (renderState.surface.drawW != localLayout.ClientArea.w ||
                    renderState.surface.drawH != localLayout.ClientArea.h) {
                    
                    renderState.surface.newW = localLayout.ClientArea.w;
                    renderState.surface.newH = localLayout.ClientArea.h;
                    renderState.surface.needResize = true;
                    needsNotify = true;
                }
            }
            if (needsNotify) {
                sharedRenderThread->Notify(); // Thông báo sau khi đã nhả lock
            }
        }
    }
    #endif

    // Cập nhật dữ liệu mới vào PropertyBag
    runtime->properties.Set<WindowLayout>("Layout", localLayout);

    return hasSizeChanged;
}
/**
 * @brief Hàm định tuyến cập nhật trạng thái tự động thông minh
 * 
 * @param runtime 
 */
inline bool RouteWindowStateUpdate(WindowRuntime* runtime) {
    if (!runtime) return false;
    // Giờ đây chỉ cần gọi một hàm duy nhất
    return UpdateWindowState(runtime);
}

inline void AdjustWindowFrameRates(WindowManager& winManager) {
    PlaybackState state = GetPlaybackState();

    for (auto& [id, windowInstance] : winManager) {
        if (!windowInstance) continue;

        // Nếu cửa sổ bị ẩn hoặc thu nhỏ, giảm FPS xuống mức tối thiểu.
        if (!windowInstance->state.display.isShown || windowInstance->state.display.isMinimized) {
            if (windowInstance->windowloop) windowInstance->windowloop->setTargetFPS(1);
            continue;
        }

        // Logic cho cửa sổ chính
        if (windowInstance->style.isMainWindow) {
            if (windowInstance->state.runtime.is_dirty) {
                // Nếu có tương tác (ví dụ: hover nút), tăng FPS để animation mượt mà.
                if (windowInstance->windowloop) windowInstance->windowloop->setTargetFPS(60);
                windowInstance->state.runtime.is_dirty = false; // Reset cờ sau khi xử lý.
            } else {
                // Logic mặc định: FPS cao khi phát video, thấp khi tạm dừng.
                if (windowInstance->windowloop) {
                    switch (state)
                    {
                    case PlaybackState::Playing:
                        windowInstance->windowloop->setTargetFPS(60);
                        break;
                    
                    case PlaybackState::Loading:
                    case PlaybackState::Seeking:
                        windowInstance->windowloop->setTargetFPS(30);
                        break;
                    
                    default:
                        windowInstance->windowloop->setTargetFPS(15);
                        break;
                    }

                }
            }
        } 
        // Logic cho các cửa sổ phụ
        else {
            if (windowInstance->state.runtime.is_dirty) {
                if (windowInstance->windowloop) windowInstance->windowloop->setTargetFPS(30); // Tăng FPS khi có tương tác.
                windowInstance->state.runtime.is_dirty = false;
            } else {
                if (windowInstance->windowloop) windowInstance->windowloop->setTargetFPS(15); // FPS thấp mặc định cho cửa sổ phụ.
            }
        }
    }
}