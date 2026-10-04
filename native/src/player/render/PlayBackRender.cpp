#include <array>
#include <thread>
#include <iostream>
#include <chrono>
#include <cstdio>

#include "log.h"
#include "player/render/PlayBackRender.h"
#include "player/player/Player.h"
#include "player/render/PlayBackRenderThread.h"
#include "backends/IGraphicsBackend.h"
#include "mpv/render_gl.h"
#include "windows/WindowUtils.h"
#include "common/LifecycleEvidence.h"

void *GetProcAddressWrapper([[maybe_unused]] void *ctx, const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

PlayBackRender::PlayBackRender() : m_render_ctx(nullptr) {}

PlayBackRender::~PlayBackRender()
{
    Shutdown();
}

#ifdef RENDER_MPV_THREAD
void PlayBackRender::HandleRenderUpdate(void* userdata) noexcept
{
    auto* callbackState = static_cast<RenderUpdateCallbackState*>(userdata);
    if (!callbackState)
        return;

    auto invocation = callbackState->lifetime.Enter();
    if (!invocation)
        return;

    const std::uint64_t callbackCount =
        callbackState->acceptedCallbacks.fetch_add(1, std::memory_order_acq_rel) + 1;

    if (callbackCount == 1)
    {
        char diag[256]{};
        const std::string stateId = LifecycleEvidence::PointerIdentity(callbackState);
        std::snprintf(
            diag,
            sizeof(diag),
            "callback_first state=%s callback_count=%llu",
            stateId.c_str(),
            static_cast<unsigned long long>(callbackCount));
        LifecycleEvidence::EmitDiagnostic("RENDER_CALLBACK", diag);
    }

    if (auto* thread = callbackState->thread.load(std::memory_order_acquire))
    {
        thread->RequestRender();
    }
}
#endif

#ifdef RENDER_MPV_THREAD
bool PlayBackRender::Init(Player &player, IGraphicsBackend *backend, const RenderThreadInitParams &threadParams)
{
#else
bool PlayBackRender::Init(Player &player, IGraphicsBackend *backend)
{
#endif
    if (m_render_ctx)
        return true;

    mpv_handle *mpv_ptr = player.GetHandle();
    if (!mpv_ptr || !backend)
        return false;

    // 1. Tạo Context MPV Render
    mpv_opengl_init_params gl_init_params{};
    gl_init_params.get_proc_address = GetProcAddressWrapper;

    std::array<mpv_render_param, 3> render_params{{{MPV_RENDER_PARAM_API_TYPE, (void *)backend->GetMpvApiType()},
                                                   {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
                                                   {MPV_RENDER_PARAM_INVALID, nullptr}}};

    if (mpv_render_context_create(&m_render_ctx, mpv_ptr, render_params.data()) < 0)
    {
        return false;
    }

    LifecycleEvidence::Emit(
        "MPVRenderContext",
        "CREATE",
        LifecycleEvidence::PointerIdentity(m_render_ctx));

#ifdef RENDER_MPV_THREAD
    // 2. Tự khởi tạo và cấu hình PlayBackRenderThread
    m_renderThread = std::make_unique<PlayBackRenderThread>();
    m_renderThread->state.mpv = threadParams.mpv;
    m_renderThread->state.ownerWindowId = threadParams.ownerWindowId;
    m_renderThread->state.window = threadParams.window;
    m_renderThread->state.render_ctx = m_render_ctx;
    m_renderThread->state.graphicsBackend = threadParams.graphicsBackend;
    m_renderThread->state.graphicsContext = threadParams.graphicsContext;
    m_renderThread->state.surface.drawW = threadParams.initialW;
    m_renderThread->state.surface.drawH = threadParams.initialH;
    m_renderThread->state.Audio_visualizers = m_audioVisualizers.load();

    // 3. Create callback state before registration. The state outlives both
    // callback detachment and render-worker destruction.
    m_updateCallbackState = std::make_unique<RenderUpdateCallbackState>();
    m_updateCallbackState->thread.store(m_renderThread.get(), std::memory_order_release);

    // 4. Start the worker before MPV can request render work.
    m_renderThread->Start();

    // 5. Register stable callback userdata. Shutdown closes this gate before
    // detaching the callback and destroying the worker.
    mpv_render_context_set_update_callback(
        m_render_ctx,
        &PlayBackRender::HandleRenderUpdate,
        m_updateCallbackState.get());
#else
    mpv_render_context_set_update_callback(m_render_ctx, [](void *)
                                           {
        SDL_Event ev;
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev); }, nullptr);
#endif

    LifecycleEvidence::Emit(
        "MPVRenderContext",
        "START",
        LifecycleEvidence::PointerIdentity(m_render_ctx));
    return true;
}

void PlayBackRender::BindStartupState(PlayerStateSystem* state)
{
#ifdef RENDER_MPV_THREAD
    if (m_renderThread)
        m_renderThread->BindStartupState(state);
#else
    (void)state;
#endif
}

void PlayBackRender::Shutdown()
{
    std::lock_guard<std::mutex> shutdownLock(m_shutdownMutex);
    if (m_shutdownComplete)
        return;

    std::string renderContextLifecycleId;
    if (m_render_ctx)
    {
        renderContextLifecycleId = LifecycleEvidence::PointerIdentity(m_render_ctx);
        LifecycleEvidence::Emit("MPVRenderContext", "STOP", renderContextLifecycleId);
    }

#ifdef RENDER_MPV_THREAD
    // Phase 1: reject all new callback work.
    if (m_updateCallbackState)
    {
        const std::uint64_t callbackCount =
            m_updateCallbackState->acceptedCallbacks.load(std::memory_order_acquire);
        const std::size_t inFlight =
            m_updateCallbackState->lifetime.InFlight();

        char preCloseDiag[320]{};
        const std::string stateId =
            LifecycleEvidence::PointerIdentity(m_updateCallbackState.get());
        std::snprintf(
            preCloseDiag,
            sizeof(preCloseDiag),
            "shutdown_pre_close state=%s callback_count=%llu in_flight=%zu",
            stateId.c_str(),
            static_cast<unsigned long long>(callbackCount),
            inFlight);
        LifecycleEvidence::EmitDiagnostic("RENDER_CALLBACK", preCloseDiag);

        m_updateCallbackState->lifetime.Close();
        LOG(1, LogLevel::Info, LogCategory::Render,
            "[RenderShutdown] callback gate closed");

        // Any callback admitted before Close() must finish before the worker
        // boundary can change.
        m_updateCallbackState->lifetime.WaitForQuiescence();

        char quiescentDiag[320]{};
        std::snprintf(
            quiescentDiag,
            sizeof(quiescentDiag),
            "shutdown_quiescent state=%s callback_count=%llu in_flight=%zu",
            stateId.c_str(),
            static_cast<unsigned long long>(
                m_updateCallbackState->acceptedCallbacks.load(
                    std::memory_order_acquire)),
            m_updateCallbackState->lifetime.InFlight());
        LifecycleEvidence::EmitDiagnostic("RENDER_CALLBACK", quiescentDiag);

        LOG(1, LogLevel::Info, LogCategory::Render,
            "[RenderShutdown] pre-stop callback quiescence established");

        // New callbacks are rejected after Close(); make worker access explicit.
        m_updateCallbackState->thread.store(nullptr, std::memory_order_release);
    }

    // Phase 2: stop + join the render worker while callback userdata is still
    // alive. Joining first also guarantees no mpv_render_context_render() is
    // active when shutdown calls another mpv_render_* function below.
    if (m_renderThread)
    {
        m_renderThread->Stop();
        LOG(1, LogLevel::Info, LogCategory::Render,
            "[RenderShutdown] render worker stopped and joined");
    }
#endif

    // Phase 3: detach MPV callback only after the render worker can no longer
    // be inside mpv_render_context_render(). libmpv requires mpv_render_*
    // functions for one context not to run concurrently.
    if (m_render_ctx)
    {
        mpv_render_context_set_update_callback(m_render_ctx, nullptr, nullptr);
        LOG(1, LogLevel::Info, LogCategory::Render,
            "[RenderShutdown] mpv update callback detached");
    }

#ifdef RENDER_MPV_THREAD
    if (m_updateCallbackState)
    {
        // Defensive wait for callbacks that raced with detachment but were
        // rejected by the closed gate.
        m_updateCallbackState->lifetime.WaitForQuiescence();
        LOG(1, LogLevel::Info, LogCategory::Render,
            "[RenderShutdown] post-detach callback quiescence established");
    }

    // Phase 4: callback is detached and no callback can access the worker.
    if (m_renderThread)
    {
        m_renderThread.reset();
        LOG(1, LogLevel::Info, LogCategory::Render,
            "[RenderShutdown] render worker destroyed");
    }
#endif

    // Phase 5: destroy the MPV render context after the render worker is gone.
    if (m_render_ctx)
    {
        mpv_render_context_free(m_render_ctx);
        m_render_ctx = nullptr;
        LifecycleEvidence::Emit("MPVRenderContext", "DESTROY", renderContextLifecycleId);
        LOG(1, LogLevel::Info, LogCategory::Render,
            "[RenderShutdown] mpv render context freed");
    }

#ifdef RENDER_MPV_THREAD
    // Callback state is the final lifetime boundary and outlives the context.
    if (m_updateCallbackState)
    {
        m_updateCallbackState->lifetime.WaitForQuiescence();
        m_updateCallbackState.reset();
        LOG(1, LogLevel::Info, LogCategory::Render,
            "[RenderShutdown] callback state destroyed");
    }
#endif

    m_shutdownComplete = true;
}

void PlayBackRender::SetAudioVisualizerEnabled(bool enable)
{
    m_audioVisualizers.store(enable);
#ifdef RENDER_MPV_THREAD
    if (m_renderThread)
    {
        m_renderThread->state.Audio_visualizers.store(enable);
    }
#endif
}

void PlayBackRender::Render(const ImVec2 &size, IGraphicsBackend *backend)
{
    if (!m_render_ctx || !backend || m_audioVisualizers.load())
        return;

#ifdef RENDER_MPV_THREAD
    if (!m_renderThread || !m_renderThread->state.fboPool)
        return;

    // Cố gắng lấy một frame mới từ pool.
    // Không cần vòng lặp ở đây, vì nếu không có frame mới, chúng ta sẽ vẽ lại frame cũ.
    FrameTextureInfo newFrame = m_renderThread->state.fboPool->GetStableFrame();

    if (newFrame.texID != nullptr)
    {
        // Trường hợp tốt nhất: có frame mới.
        if (newFrame.frameId > m_lastDisplayedFrame.frameId)
        {
            m_lastDisplayedFrame = newFrame;
            m_lastFrameTime = std::chrono::steady_clock::now(); // Cập nhật timestamp
        }
        else if (newFrame.frameId > 0 && newFrame.frameId == m_lastDisplayedFrame.frameId)
        {
            // Cảnh báo: GetStableFrame trả về cùng một frame. Điều này có thể xảy ra
            // nếu UI render nhanh hơn video FPS, không phải lỗi nghiêm trọng.
            // LOG(1, "[RenderUI] DEBUG: GetStableFrame returned the same frameId (%llu).", newFrame.frameId);
        }
        else if (newFrame.frameId > 0 && newFrame.frameId < m_lastDisplayedFrame.frameId)
        {
            // Cảnh báo nghiêm trọng: GetStableFrame trả về một frame CŨ HƠN.
            // Điều này cho thấy có thể có lỗi logic trong việc chọn frame mới nhất trong FBO pool.
            LOG(1, LogLevel::Warning, LogCategory::Render, "[RenderUI] CRITICAL WARN: GetStableFrame returned an OLDER frameId (%d) than currently displayed (%d).", newFrame.frameId, m_lastDisplayedFrame.frameId);
        }
    }

    // --- Logic kiểm tra timeout ---
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameTime);

    // Nếu không có frame mới trong hơn 500ms, coi như video đã dừng/lỗi và xóa frame cũ.
    if (m_lastDisplayedFrame.texID != nullptr && elapsed.count() > 500)
    {
        LOG(500, LogLevel::Info, LogCategory::Render, "[RenderUI] INFO: Stale frame timeout. Clearing last displayed frame (ID: %d).", m_lastDisplayedFrame.frameId);
    }

    // Luôn vẽ frame cuối cùng hợp lệ, dù nó là frame mới hay frame cũ từ lần trước.
    if (m_lastDisplayedFrame.texID != nullptr)
    {
        ImGui::Image(static_cast<void *>(m_lastDisplayedFrame.texID), size, ImVec2(0, 0), ImVec2(m_lastDisplayedFrame.u, m_lastDisplayedFrame.v));
    }
#else
    auto params = backend->GetPlayBackRenderParams(size);
    mpv_render_context_render(m_render_ctx, params.data());
#endif
}