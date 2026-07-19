#include "MPVRender.h"
#include "mpv/player/MPVPlayer.h"
#include "mpv/render/MPVRenderThread.h"
#include "mpv/session/MPVManager.h"
#include "utils.h"
#include "backends/IGraphicsBackend.h" // Thêm include
#include <mpv/render_gl.h> // Vẫn cần cho mpv_opengl_init_params
#include <array>
#include "utils.h"
#include "globals.h"
#include "windows/utils.h"

void* GetProcAddressWrapper([[maybe_unused]] void* ctx, const char* name) {
    return SDL_GL_GetProcAddress(name);
}

MPVRender::MPVRender() : m_render_ctx(nullptr) {}

MPVRender::~MPVRender() {
    Shutdown();
}

bool MPVRender::Init(MPVPlayer& player, IGraphicsBackend* backend) {
    if (m_render_ctx) return true;

    mpv_handle* mpv_ptr = player.GetHandle();
    if (!mpv_ptr || !backend) return false;

    // Các tham số này vẫn là của OpenGL, nhưng có thể được mở rộng sau này
    mpv_opengl_init_params gl_init_params{};
    gl_init_params.get_proc_address = GetProcAddressWrapper;

    std::array<mpv_render_param, 3> render_params{{
        { MPV_RENDER_PARAM_API_TYPE, (void*)backend->GetMpvApiType() },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    }};
    
    if (mpv_render_context_create(&m_render_ctx, mpv_ptr, render_params.data()) < 0) {
        return false;
    }

#ifdef RENDER_MPV_THREAD
    mpv_render_context_set_update_callback(m_render_ctx, [](void* userdata) {
        auto* session = MPVManager::GetInstance().GetDefaultSession();
        if (session && session->GetRenderThread()) session->GetRenderThread()->RequestRender();
    }, nullptr);
#else
    mpv_render_context_set_update_callback(m_render_ctx, [](void*) {
        SDL_Event ev;
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev);
    }, nullptr);
#endif

    return true;
}

void MPVRender::Shutdown() {
    if (m_render_ctx) {
        mpv_render_context_free(m_render_ctx);
        m_render_ctx = nullptr;
    }
}

void MPVRender::Render(const ImVec2& size, IGraphicsBackend* backend) {
    if (!m_render_ctx || !backend || Audio_visualizers) return;

    // Lấy con trỏ tới session để truy cập luồng render
    auto* session = MPVManager::GetInstance().GetDefaultSession();
    if (!session) return;

#ifdef RENDER_MPV_THREAD
    if (!session || !session->GetRenderThread() || !session->GetRenderThread()->state.fboPool) return;

    FrameTextureInfo frameInfo;
    // Thử lấy frame ổn định vài lần với một khoảng nghỉ ngắn để chờ luồng render hoàn thành
    for (int i = 0; i < 3; ++i) {
        frameInfo = session->GetRenderThread()->state.fboPool->GetStableFrame();
        if (frameInfo.texID.has_value()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (frameInfo.texID.has_value()) {
        ImGui::Image(std::any_cast<ImTextureID>(frameInfo.texID), size, ImVec2(0, 0), ImVec2(frameInfo.u, frameInfo.v));
    }
#else
    // Lấy tham số render từ backend và gọi render
    auto params = backend->GetMpvRenderParams(size);
    mpv_render_context_render(m_render_ctx, params.data());
#endif
}