#pragma once

#include <mpv/client.h>
#include <mpv/render.h>
#include <imgui.h>
#include <memory>
#include <atomic>
class Player; // Forward declaration
class IGraphicsBackend;
class PlayBackRenderThread;
class PlayBackRender {
public:
    PlayBackRender();
    ~PlayBackRender();

    bool Init(Player& player, IGraphicsBackend* backend, std::shared_ptr<PlayBackRenderThread> renderThread = nullptr);
    void Shutdown();

    void Render(const ImVec2& size, IGraphicsBackend* backend);

    bool IsAudioVisualizerEnabled() const { 
        return m_audioVisualizers.load(); 
    }

    void SetAudioVisualizerEnabled(bool enable) {
        m_audioVisualizers.store(enable);
        // Đồng bộ sang Thread state nếu đang chạy chế độ RENDER_MPV_THREAD
#ifdef RENDER_MPV_THREAD
        if (auto thread = m_renderThread.lock()) {
            thread->state.Audio_visualizers.store(enable);
        }
#endif
    }

    mpv_render_context* GetContext() const { return m_render_ctx; }

private:
    mpv_render_context* m_render_ctx = nullptr;
    std::atomic<bool> m_audioVisualizers{false};
#ifdef RENDER_MPV_THREAD
    std::weak_ptr<PlayBackRenderThread> m_renderThread; // Dùng weak_ptr để tránh vòng lặp tham chiếu (circular reference)
#endif
};