#include "MPVRenderThread.h"
#include "mpv/render/IFrameBufferPool.h"
#include "threads/thread_manager.h"
#include "mpv/session/MPVManager.h"
#include "WindowRuntime.h"
#include "mpv/render/MPVRender.h"
#include "mpv/render_gl.h"
#include <gl3w.h> // Thêm header cho GLuint
#include "utils.h"
#include <array>
#include "windows/WindowUtils.h"
MPVRenderThread::MPVRenderThread() {}

MPVRenderThread::~MPVRenderThread() {
    Stop();
}

void MPVRenderThread::Start() {
    if (m_running) return;

    m_running = true;
    m_thread = std::thread(&MPVRenderThread::Run, this);
    GetThreadManager().Register(ThreadID::MPVRenderThread, &m_thread);
}

void MPVRenderThread::Stop() {
    if (!m_running) return;

    m_running = false;
    state.cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    GetThreadManager().Unregister(ThreadID::MPVRenderThread);
}

void MPVRenderThread::RequestRender() {
    {
        std::lock_guard lock(state.mtx);
        state.needRender = true;
    }
    Notify();
}

void MPVRenderThread::Notify() {
    state.cv.notify_one(); // Chỉ thông báo, không khóa
}

void MPVRenderThread::SetVideoSize(int w, int h) {
    auto* session = MPVManager::GetInstance().GetDefaultSession();
    if (!session || !session->GetRenderThread()) return;
    auto& renderState = session->GetRenderThread()->state;
    if (w != renderState.surface.drawW || h != renderState.surface.drawH) {
        renderState.surface.newW = w;
        renderState.surface.newH = h;
        renderState.surface.needResize = true;
        RequestRender();
    }
}

void MPVRenderThread::Run() {
    if (!this->state.graphicsBackend) {
        state.hasExited = true;
        return;
    }

    // Sử dụng trực tiếp 'this->state' thay vì truy vấn qua Manager
    if (!this->state.graphicsBackend->MakeCurrent(this->state.window, this->state.graphicsContext)) {
        SDL_Log("Lỗi: Không thể bind MPV Sub-Context lên Render Thread!");
        state.hasExited = true;
        return;
    }
    SDL_GL_SetSwapInterval(1);

    // Sử dụng factory method để tạo FBO pool tương ứng
    this->state.fboPool = this->state.graphicsBackend->CreateFrameBufferPool();
    if (this->state.fboPool) {
        this->state.fboPool->Init(this->state.graphicsBackend);
    }

    FrameTimer framerender(30);

    while (m_running) {
        std::unique_lock lock(this->state.mtx);
        this->state.cv.wait(lock, [&] {
            return this->state.needRender || !m_running;
        });

        if (!m_running) break;

        this->state.needRender = false;
        
        if (this->state.surface.needResize) {
            this->state.surface.drawW = this->state.surface.newW;
            this->state.surface.drawH = this->state.surface.newH;
            this->state.surface.needResize = false;
        }

        lock.unlock();

        bool isVisible = this->state.g_WindowVisible.load(std::memory_order_relaxed);
        bool isVisualizer = this->state.Audio_visualizers.load(std::memory_order_relaxed);
        bool isZeroSize = (this->state.surface.drawW <= 0 || this->state.surface.drawH <= 0);
        int skip_render = (!isVisible || isVisualizer || isZeroSize) ? 1 : 0;

        if (skip_render) {
            // Thậm chí khi skip, chúng ta vẫn cần gọi render để mpv xử lý các sự kiện nội bộ
            std::array<mpv_render_param, 2> skip_params {{{ MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render }, { MPV_RENDER_PARAM_INVALID, nullptr }}};
            mpv_render_context_render(this->state.render_ctx, skip_params.data());
            continue; 
        }

        if (!this->state.fboPool) continue; // Không có pool thì không render

        int index = this->state.fboPool->AcquireFreeBuffer();
        FrameNode& frame = this->state.fboPool->GetFrame(index);

        int targetW = state.surface.drawW;
        int targetH = state.surface.drawH;

        state.fboPool->ResizeFrame(index, targetW, targetH);
        frame.contentW = std::min(targetW, frame.allocatedW);
        frame.contentH = std::min(targetH, frame.allocatedH);

        // Lấy tham số render từ backend, thay vì tạo mpv_opengl_fbo cứng
        // Đây là một ví dụ, bạn sẽ cần một cơ chế tốt hơn để truyền FBO/Texture
        // từ FrameBufferPool vào backend để nó tạo ra các tham số đúng.
        std::vector<mpv_render_param> params;
        if (this->state.graphicsBackend->GetMpvApiType() == std::string("opengl")) {
            mpv_opengl_fbo fbo{};
            fbo.fbo = frame.fbo.has_value() ? static_cast<int>(std::any_cast<GLuint>(frame.fbo)) : 0; // Giữ nguyên vì mpv_opengl_fbo::fbo là int
            fbo.w = frame.contentW;
            fbo.h = frame.contentH;
            fbo.internal_format = this->state.graphicsBackend->GetGLInternalFormat();
            params.push_back({MPV_RENDER_PARAM_OPENGL_FBO, &fbo});
        }

        int flip = 0;
        params.push_back({MPV_RENDER_PARAM_FLIP_Y, &flip});
        params.push_back({MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render});
        params.push_back({MPV_RENDER_PARAM_INVALID, nullptr});
        mpv_render_context_render(this->state.render_ctx, params.data());

        this->state.fboPool->MarkAsReady(index);
        this->state.framerender.store(framerender.updateAndGetFPS());

        SDL_Event ev;
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev);
    }

    if (state.fboPool) state.fboPool->Shutdown();
    state.hasExited = true;
}