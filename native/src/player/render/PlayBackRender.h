#pragma once

#include <mpv/client.h>
#include <mpv/render.h>
#include <imgui.h>
#include <memory>
#include <atomic>
#include <any>
#include <SDL.h>
#include <chrono>

class Player;
class IGraphicsBackend;
class PlayBackRenderThread;

#ifdef RENDER_MPV_THREAD
#include "player/render/IFrameBufferPool.h"
// Struct đóng gói dữ liệu khởi tạo cho Render Thread
struct RenderThreadInitParams {
    mpv_handle* mpv = nullptr;
    uint32_t ownerWindowId = 0;
    SDL_Window* window = nullptr;
    IGraphicsBackend* graphicsBackend = nullptr;
    std::any graphicsContext; // Sub-Context OpenGL/DirectX
    int initialW = 0;
    int initialH = 0;
};
#endif

class PlayBackRender {
public:
    PlayBackRender();
    ~PlayBackRender();

#ifdef RENDER_MPV_THREAD
    bool Init(Player& player, IGraphicsBackend* backend, const RenderThreadInitParams& threadParams);
#else
    bool Init(Player& player, IGraphicsBackend* backend);
#endif

    void Shutdown();
    void Render(const ImVec2& size, IGraphicsBackend* backend);

    bool IsAudioVisualizerEnabled() const { return m_audioVisualizers.load(); }
    void SetAudioVisualizerEnabled(bool enable);

    mpv_render_context* GetContext() const { return m_render_ctx; }
    
    // Getter truy cập luồng nếu UI hoặc Session cần đọc state FPS/FBO
    PlayBackRenderThread* GetRenderThread() const {
#ifdef RENDER_MPV_THREAD
        return m_renderThread.get();
#else
        return nullptr;
#endif
    }

private:
    mpv_render_context* m_render_ctx = nullptr;
    std::atomic<bool> m_audioVisualizers{false};

#ifdef RENDER_MPV_THREAD
    // PlayBackRender SỞ HỮU HOÀN TOÀN luồng này
    FrameTextureInfo m_lastDisplayedFrame;
    std::chrono::steady_clock::time_point m_lastFrameTime;
    std::unique_ptr<PlayBackRenderThread> m_renderThread;
#endif
};