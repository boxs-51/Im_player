
#include "threads/thread_manager.h"
#include <atomic>
#include "player/render/PlayBackRender.h"
#include "player/render/PlayBackRenderThread.h"
#include "log.h" // Thêm header cho LOG
#include "player/player/Player.h"

#include "mpv/render_gl.h"
#include <gl3w.h> // Thêm header cho GLuint
#include "utils.h"
#include <array>
#include "common/Exception.h"
#include "common/LifecycleEvidence.h"
#include "windows/WindowUtils.h"
PlayBackRenderThread::PlayBackRenderThread() {
    LifecycleEvidence::Emit(
        "PlayBackRenderThread",
        "CREATE",
        LifecycleEvidence::PointerIdentity(this));
}

PlayBackRenderThread::~PlayBackRenderThread() {
    Stop();
    LifecycleEvidence::Emit(
        "PlayBackRenderThread",
        "DESTROY",
        LifecycleEvidence::PointerIdentity(this));
}

void PlayBackRenderThread::Start() {
    if (m_running) return;

    m_running = true;
    m_thread = std::thread(&PlayBackRenderThread::Run, this);
    m_registeredThreadName = "PlayBackRenderThread_" + (m_sessionId.empty() ? std::to_string(reinterpret_cast<uintptr_t>(this)) : m_sessionId);
    GetThreadManager().Register(m_registeredThreadName, &m_thread);
    LifecycleEvidence::Emit("PlayBackRenderThread", "START", LifecycleEvidence::PointerIdentity(this));
}

void PlayBackRenderThread::Stop() {
    if (!m_running) return;

    const std::string lifecycleId = LifecycleEvidence::PointerIdentity(this);
    LifecycleEvidence::Emit("PlayBackRenderThread", "STOP", lifecycleId);
    m_running = false;
    state.cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
        LifecycleEvidence::Emit("PlayBackRenderThread", "JOIN", lifecycleId);
    }

    if (!m_registeredThreadName.empty()) {
        GetThreadManager().Unregister(m_registeredThreadName);
        m_registeredThreadName.clear();
    }
}

void PlayBackRenderThread::RequestRender() {
    {
        std::lock_guard lock(state.mtx);
        state.needRender = true;
    }
    Notify();
}

void PlayBackRenderThread::Notify() {
    state.cv.notify_one(); // Chỉ thông báo, không khóa
}

void PlayBackRenderThread::SetVideoSize(int w, int h) {
    std::lock_guard lock(state.mtx);
    if (w != state.surface.drawW || h != state.surface.drawH) {
        state.surface.newW = w;
        state.surface.newH = h;
        state.surface.needResize = true;
    }
    Notify();
}

void PlayBackRenderThread::Run() {
try {
    if (!this->state.graphicsBackend) {
        state.hasExited = true;
        return;
    }

    if (!this->state.graphicsBackend->MakeCurrent(this->state.window, this->state.graphicsContext)) {
        LOG(1, LogLevel::Error, LogCategory::Render, "[RenderThread] ERROR: Failed to bind MPV Sub-Context to Render Thread!");
        state.hasExited = true;
        return;
    }
    SDL_GL_SetSwapInterval(0);

    this->state.fboPool = this->state.graphicsBackend->CreateFrameBufferPool();
    if (this->state.fboPool) {
        this->state.fboPool->Init(this->state.graphicsBackend);
    }

    FrameTimer framerender(60);

    // Dùng vector tối ưu bộ nhớ cố định ngoài vòng lặp
    std::vector<mpv_render_param> params;
    params.reserve(5);

    while (m_running) {
        bool shouldRender = false;
        int localDrawW = 0;
        int localDrawH = 0;

        // --- 1. Chờ sự kiện và xử lý trạng thái dưới lock ---
        {
            std::unique_lock lock(this->state.mtx);
            this->state.cv.wait(lock, [&] {
                // Thức dậy nếu cần render, resize, hoặc dừng luồng
                return this->state.needRender || this->state.surface.needResize || !m_running;
            });

            if (!m_running) break;

            // Luôn xử lý yêu cầu resize nếu có
            if (this->state.surface.needResize) {
                this->state.surface.drawW = this->state.surface.newW;
                this->state.surface.drawH = this->state.surface.newH;
                this->state.surface.needResize = false;
            }

            // Chỉ render nếu có yêu cầu `needRender`
            if (this->state.needRender) {
                shouldRender = true;
                this->state.needRender = false; // Reset cờ sau khi đã nhận yêu cầu

                // Sao chép kích thước ra biến cục bộ để đảm bảo nhất quán trong lần render này
                localDrawW = this->state.surface.drawW;
                localDrawH = this->state.surface.drawH;
            }
        } // Khóa được giải phóng tại đây

        // Nếu chỉ thức dậy để resize mà không cần render, quay lại vòng lặp chờ
        if (!shouldRender) {
            continue;
        }

        framerender.startFrame();

        // --- 2. Thực hiện render bằng các biến cục bộ ---
        bool isVisible = this->state.g_WindowVisible.load(std::memory_order_relaxed);
        bool isVisualizer = this->state.Audio_visualizers.load(std::memory_order_relaxed);
        bool isZeroSize = (localDrawW <= 0 || localDrawH <= 0);
        int skip_render = (!isVisible || isVisualizer || isZeroSize) ? 1 : 0;

        if (skip_render) {
            std::array<mpv_render_param, 2> skip_params {{{ MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render }, { MPV_RENDER_PARAM_INVALID, nullptr }}};
            mpv_render_context_render(this->state.render_ctx, skip_params.data());
            continue; 
        }

        if (!this->state.fboPool) continue;

        int index = this->state.fboPool->AcquireFreeBuffer();
        m_lastAcquiredBufferId.store(index, std::memory_order_release);

        if (index == -1) {
            m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
            // Log này rất quan trọng để biết tại sao video bị đứng
            LOG(100, LogLevel::Warning, LogCategory::Render, "[RenderThread] WARN: Failed to acquire buffer, dropping frame. Total dropped: %d", m_droppedFrames.load());
            framerender.endFrame();
            continue; 
        }
        FrameNode& frame = this->state.fboPool->GetFrame(index);
        int targetW = localDrawW;
        int targetH = localDrawH;

        state.fboPool->ResizeFrame(index, targetW, targetH);
        frame.contentW = std::min(targetW, frame.allocatedW);
        frame.contentH = std::min(targetH, frame.allocatedH);

        params.clear();

        mpv_opengl_fbo fbo{};
        if (this->state.graphicsBackend->GetMpvApiType() == std::string("opengl")) {
            fbo.fbo = static_cast<int>(frame.fbo);
            fbo.w = frame.contentW;
            fbo.h = frame.contentH;
            fbo.internal_format = this->state.graphicsBackend->GetGLInternalFormat();
            params.push_back({MPV_RENDER_PARAM_OPENGL_FBO, &fbo});
        }

        int flip = 0;
        params.push_back({MPV_RENDER_PARAM_FLIP_Y, &flip});
        params.push_back({MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render});
        params.push_back({MPV_RENDER_PARAM_INVALID, nullptr});

        // Gọi Render MPV
        mpv_render_context_render(this->state.render_ctx, params.data());

        uint64_t currentFrameId = ++m_frameCounter;
        this->state.fboPool->MarkAsReady(index, currentFrameId);
        m_lastRenderedFrameId.store(currentFrameId, std::memory_order_release);
        this->state.framerender.store(framerender.getFPS());

        SDL_Event ev;
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev);
        framerender.endFrame();
    }

    if (this->state.fboPool) this->state.fboPool->Shutdown();
    state.hasExited = true;
} catch (const std::exception& e) {
    LOG(1, LogLevel::Critical, LogCategory::Render, "[RenderThread] CRITICAL ERROR: Exception in PlayBackRenderThread: %s", e.what());
} catch (...) {
    LOG(1, LogLevel::Critical, LogCategory::Render, "[RenderThread] CRITICAL ERROR: Unknown exception in PlayBackRenderThread.");
}
}