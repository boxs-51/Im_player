#include <array>
#include <thread>
#include <iostream>
#include <chrono>

#include "log.h"
#include "player/render/PlayBackRender.h"
#include "player/player/Player.h"
#include "player/render/PlayBackRenderThread.h"
#include "backends/IGraphicsBackend.h"
#include "mpv/render_gl.h"
#include "windows/WindowUtils.h"

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

    // 3. Khởi chạy luồng
    m_renderThread->Start();

    // 4. Set Callback MPV - Truyền trực tiếp raw pointer `m_renderThread.get()`
    mpv_render_context_set_update_callback(m_render_ctx, [](void *userdata)
                                           {
        auto* thread = static_cast<PlayBackRenderThread*>(userdata);
        if (thread) {
            thread->RequestRender();
        } }, m_renderThread.get());
#else
    mpv_render_context_set_update_callback(m_render_ctx, [](void *)
                                           {
        SDL_Event ev;
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev); }, nullptr);
#endif

    return true;
}

void PlayBackRender::Shutdown()
{
    if (m_render_ctx)
    {
        // TẮT CALLBACK MPV TRƯỚC HẾT để ngắt kết nối với luồng
        mpv_render_context_set_update_callback(m_render_ctx, nullptr, nullptr);
        mpv_render_context_free(m_render_ctx);
        m_render_ctx = nullptr;
    }

#ifdef RENDER_MPV_THREAD
    // Tự dọn dẹp Luồng Render do mình sở hữu
    if (m_renderThread)
    {
        m_renderThread->Stop();
        m_renderThread.reset(); // Xóa hoàn toàn instance
    }
#endif
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
            // LOG_NO_KEY(1, "[RenderUI] DEBUG: GetStableFrame returned the same frameId (%llu).", newFrame.frameId);
        }
        else if (newFrame.frameId > 0 && newFrame.frameId < m_lastDisplayedFrame.frameId)
        {
            // Cảnh báo nghiêm trọng: GetStableFrame trả về một frame CŨ HƠN.
            // Điều này cho thấy có thể có lỗi logic trong việc chọn frame mới nhất trong FBO pool.
            LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Render, std::cout << "[RenderUI] CRITICAL WARN: GetStableFrame returned an OLDER frameId (" << newFrame.frameId << ") than currently displayed (" << m_lastDisplayedFrame.frameId << ").");
        }
    }

    // --- Logic kiểm tra timeout ---
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameTime);

    // Nếu không có frame mới trong hơn 500ms, coi như video đã dừng/lỗi và xóa frame cũ.
    if (m_lastDisplayedFrame.texID != nullptr && elapsed.count() > 500)
    {
        LOG_NO_KEY(500, LogLevel::Info, LogCategory::Render, std::cout << "[RenderUI] INFO: Stale frame timeout. Clearing last displayed frame (ID: " << m_lastDisplayedFrame.frameId << ").");
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