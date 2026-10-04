#pragma once

#include <mpv/client.h>
#include <mpv/render.h>
#include <imgui.h>
#include <memory>
#include <atomic>
#include <cstdint>
#include <any>
#include <SDL.h>
#include <chrono>
#include <mutex>

#include "player/render/RenderCallbackLifetimeGate.h"

class Player;
class PlayerStateSystem;
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
    void BindStartupState(PlayerStateSystem* state);

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
#ifdef RENDER_MPV_THREAD
    struct RenderUpdateCallbackState {
        RenderCallbackLifetimeGate lifetime;
        std::atomic<PlayBackRenderThread*> thread{nullptr};
        std::atomic<std::uint64_t> acceptedCallbacks{0};
    };

    static void HandleRenderUpdate(void* userdata) noexcept;
#endif

    mpv_render_context* m_render_ctx = nullptr;
    std::atomic<bool> m_audioVisualizers{false};

#ifdef RENDER_MPV_THREAD
    // PlayBackRender SỞ HỮU HOÀN TOÀN luồng này
    FrameTextureInfo m_lastDisplayedFrame;
    std::chrono::steady_clock::time_point m_lastFrameTime;
    std::unique_ptr<PlayBackRenderThread> m_renderThread;
    std::unique_ptr<RenderUpdateCallbackState> m_updateCallbackState;
#endif

    std::mutex m_shutdownMutex;
    bool m_shutdownComplete = false;
};