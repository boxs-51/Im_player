// UpdateWindowState.h
#pragma once
#include "WindowRuntime.h"
#include "WindowManager.h"
#include "WindowResource.h"
#include "UIRenderThread.h"

#include "player/session/PlayerSession.h"
#include "player/PlayerStateSystem.h"
#include "player/render/PlayBackRenderThread.h"

#include <SDL.h>

#ifdef RENDER_MPV_THREAD
#include "globals.h" // Chứa thông tin biến renderThread toàn cục của bạn
#endif



inline bool UpdateWindowState(WindowRuntime* runtime) {
    if (!runtime || !runtime->resource.sdlWindow) return false;


    // Tạo các biến cục bộ để xử lý và so sánh
    WindowLayout localLayout;
    bool hasSizeChanged = false;
    WindowLayout oldLayout;
    bool isFullscreen = false;
    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);

        localLayout.WinX = runtime->state.geometry.x;
        localLayout.WinY = runtime->state.geometry.y;
        localLayout.WinW = runtime->state.geometry.width;
        localLayout.WinH = runtime->state.geometry.height;

        oldLayout = runtime->state.geometry.layout;

        runtime->state.display.isVisible = runtime->state.display.isShown && !runtime->state.display.isMinimized;

        isFullscreen = runtime->state.display.isFullscreen;

        runtime->state.stateVersion ++;

    }

    bool use_viewpoint = false;
    if (!use_viewpoint) {
        localLayout.WinX = 0;
        localLayout.WinY = 0;
    }

    // Bước 2: Tính toán Hình học Layout trên biến cục bộ
    if (isFullscreen) {
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

    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.geometry.layout = localLayout;
    }

    // Kiểm tra sự thay đổi kích thước dựa trên Layout cũ (nếu có)
    if (oldLayout.ClientSize.x != localLayout.ClientSize.x || oldLayout.ClientSize.y != localLayout.ClientSize.y) {
        hasSizeChanged = true;
        // Yêu cầu luồng UI thay đổi kích thước viewport đồ họa một cách an toàn
        if (runtime->resource.uiRenderThread) {
            runtime->resource.uiRenderThread->RequestResize(localLayout.ClientArea.w, localLayout.ClientArea.h);
        }
    }
    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.runtime.is_dirty = hasSizeChanged;
        runtime->state.runtime.change_size = hasSizeChanged;
       
    }

    // Bước 3: ĐỒNG BỘ SANG LUỒNG MPV
    #ifdef RENDER_MPV_THREAD
    if (runtime->resource.GetPlayerSession() && runtime->style.isMainWindow) {


        if (auto* sharedRenderThread = runtime->resource.GetPlayerSession()->GetRenderThread()) {
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

    return hasSizeChanged;
}
/**
 * @brief Hàm định tuyến cập nhật trạng thái tự động thông minh
 * 
 * @param runtime 
 */

inline void AdjustWindowFrameRates(WindowRuntime* runtime) {

    if (!runtime) return;

    bool isShown = false;
    bool isMinimized = false;
    bool isFullscreen = false;
    bool isDirty = false;
    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        isShown = runtime->state.display.isShown;
        isMinimized = runtime->state.display.isMinimized;
        isFullscreen = runtime->state.display.isFullscreen;
        isDirty = runtime->state.runtime.is_dirty;
    }

    // Nếu cửa sổ bị ẩn hoặc thu nhỏ, giảm FPS xuống mức tối thiểu.
    if (!isShown || isMinimized) {
        if (runtime->windowloop) runtime->windowloop->setTargetFPS(1);
        return;
    }

    // Logic cho cửa sổ chính
    if (runtime->style.isMainWindow) {
        if (isDirty) {
            // Nếu có tương tác (ví dụ: hover nút), tăng FPS để animation mượt mà.
            if (runtime->windowloop) runtime->windowloop->setTargetFPS(60);
            isDirty = false; // Reset cờ sau khi xử lý.
        } else {
            // Logic mặc định: FPS cao khi phát video, thấp khi tạm dừng.
            if (runtime->windowloop) {
                if (auto* sesion = runtime->resource.GetPlayerSession()) {
                    if (auto* player_state = sesion->GetState()) {
                        PlaybackState state = player_state->GetPlaybackState();
                        switch (state)
                        {
                        case PlaybackState::Playing:
                            runtime->windowloop->setTargetFPS(90);
                            break;
                        
                        case PlaybackState::Loading:
                        case PlaybackState::Seeking:
                            runtime->windowloop->setTargetFPS(30);
                            break;
                        
                        default:
                            runtime->windowloop->setTargetFPS(15);
                            break;
                        }
                    }
                }
            }
        }
    } 
    // Logic cho các cửa sổ phụ
    else {
        if (isDirty) {
            if (runtime->windowloop) runtime->windowloop->setTargetFPS(30); // Tăng FPS khi có tương tác.
            isDirty = false;
        } else {
            if (runtime->windowloop) runtime->windowloop->setTargetFPS(15); // FPS thấp mặc định cho cửa sổ phụ.
        }
    }

    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.runtime.is_dirty = isDirty;
    }
  
}