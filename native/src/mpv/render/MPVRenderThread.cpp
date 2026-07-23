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
#include "common/Exception.h"
#include "windows/WindowUtils.h"
MPVRenderThread::MPVRenderThread() {}

MPVRenderThread::~MPVRenderThread() {
    Stop();
}

void MPVRenderThread::Start() {
    if (m_running) return;

    m_running = true;
    m_thread = std::thread(&MPVRenderThread::Run, this);
    GetThreadManager().Register("MPVRenderThread", &m_thread);
}

void MPVRenderThread::Stop() {
    if (!m_running) return;

    m_running = false;
    state.cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    GetThreadManager().Unregister("MPVRenderThread");
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
try {
    if (!this->state.graphicsBackend) {
        state.hasExited = true;
        return;
    }

    if (!this->state.graphicsBackend->MakeCurrent(this->state.window, this->state.graphicsContext)) {
        SDL_Log("Lỗi: Không thể bind MPV Sub-Context lên Render Thread!");
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
        FrameNode& frame = this->state.fboPool->GetFrame(index);

        int targetW = localDrawW;
        int targetH = localDrawH;

        state.fboPool->ResizeFrame(index, targetW, targetH);
        frame.contentW = std::min(targetW, frame.allocatedW);
        frame.contentH = std::min(targetH, frame.allocatedH);

        params.clear();

        mpv_opengl_fbo fbo{};
        if (this->state.graphicsBackend->GetMpvApiType() == std::string("opengl")) {
            fbo.fbo = frame.fbo.has_value() ? static_cast<int>(std::any_cast<GLuint>(frame.fbo)) : 0;
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

        this->state.fboPool->MarkAsReady(index);
        this->state.framerender.store(framerender.getFPS());

        SDL_Event ev;
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev);
        framerender.endFrame();
    }

    if (state.fboPool) state.fboPool->Shutdown();
    state.hasExited = true;
} catch (const std::exception& e) {
    SDL_Log("Exception in MPVRenderThread: %s", e.what());
} catch (...) {
    SDL_Log("Unknown exception in MPVRenderThread.");
}
}