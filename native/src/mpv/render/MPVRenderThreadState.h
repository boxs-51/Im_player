// mpv_instance.h
#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <any>
#include <memory>

#include "mpv/client.h"
#include "mpv/render.h"
#include "render/IFrameBufferPool.h"

// Forward declaration để tránh include vòng lặp và giảm phụ thuộc header
class IGraphicsBackend;

// Đóng gói toàn bộ tài nguyên của 1 thực thể Player độc lập
class MPVRenderThreadState {
public:
    mpv_handle* mpv = nullptr;
    mpv_render_context* render_ctx = nullptr; // Sẽ được gán từ MPVRender
    
    // Trạng thái Render Thread phụ
    std::atomic<bool> running{ false };
    std::atomic<bool> needRender{ false };
    std::atomic<bool> hasExited{ false };
    std::atomic<bool> g_WindowVisible{ true };
    std::atomic<bool> Audio_visualizers{ false };

    std::atomic<float> framerender{ 0 };

    std::mutex mtx;
    std::condition_variable cv;
    
    // Window & Graphic Context link
    std::string ownerWindowId;
    struct SDL_Window* window = nullptr;
    std::any graphicsContext;
    IGraphicsBackend* graphicsBackend = nullptr; // Con trỏ tới backend đồ họa, không sở hữu

    // Kích thước bề mặt render
    struct {
        int drawW = 0;
        int drawH = 0;
        int newW = 0;
        int newH = 0;
        bool needResize = false;
    } surface;

    // Quản lý Frame Buffer Pool
    std::unique_ptr<IFrameBufferPool> fboPool;
};