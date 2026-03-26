// mpv_render_video.cpp
#include "mpv/mpv_render_video.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "utils.h"
#include <mpv/render_gl.h>
#include "globals.h"


#include <string>
#include <optional>
#include <array>


extern mpv_render_context* render_ctx;
extern mpv_handle* mpv;

// Thay vì #define, dùng constexpr để có type-safety
constexpr auto PARAM_FRAMEBUFFER_SIZE = static_cast<mpv_render_param_type>(3);
constexpr auto PARAM_FLIP_Y = static_cast<mpv_render_param_type>(4);

// Callback function với signature chuẩn
void* GetProcAddressWrapper([[maybe_unused]] void* ctx, const char* name) {
    return SDL_GL_GetProcAddress(name);
}

bool InitMPV(mpv_handle*& mpv_ptr) {
    mpv_ptr = mpv_create();
    if (!mpv_ptr) return false;

    ApplyStaticMPVConfig(mpv_ptr);

    if (mpv_initialize(mpv_ptr) < 0) {
        mpv_terminate_destroy(mpv_ptr);
        return false;
    }

    LoadAllScripts(mpv_ptr);
    mpv_request_log_messages(mpv_ptr, "v");
    InitMPVObservers(mpv_ptr);
    
    return true;
}

bool InitMPVRenderContext(mpv_handle* mpv_ptr) {
    if (!mpv_ptr) return false;

    // C++17: Khởi tạo struct theo cách truyền thống (tương thích tốt hơn C99 designated initializers)
    mpv_opengl_init_params gl_init_params {};
    gl_init_params.get_proc_address = GetProcAddressWrapper;
    gl_init_params.get_proc_address_ctx = nullptr;

    // Định nghĩa các tham số render
    std::array<mpv_render_param, 3> render_params {{
        { MPV_RENDER_PARAM_API_TYPE, const_cast<char*>("opengl") },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    }};

    if (mpv_render_context_create(&render_ctx, mpv_ptr, render_params.data()) < 0)
        return false;

    // Sử dụng Lambda capture nếu cần, nhưng ở đây dùng static callback của MPV
    mpv_render_context_set_update_callback(render_ctx, [](void*) {
    
        SDL_Event event;
        event.type = SDL_MPV_RENDER_UPDATE;
        SDL_PushEvent(&event);
    }, nullptr);

    mpv_set_wakeup_callback(mpv_ptr, [](void*) {
        SDL_Event event;
        event.type = SDL_MPV_EVENT;
        SDL_PushEvent(&event);
    }, nullptr);

    return true;
}

void RenderMPVVideo(const ImVec2& size) {
    if (!render_ctx) return;

    // Cấu hình Framebuffer Object
    mpv_opengl_fbo fbo {};
    fbo.fbo = 0;
    fbo.w = static_cast<int>(size.x);
    fbo.h = static_cast<int>(size.y);
    fbo.internal_format = GL_RGBA;

    int flip = 1; 
    std::array<int, 2> fb_size = { static_cast<int>(size.x), static_cast<int>(size.y) };

    // Tổ chức params bằng std::array để quản lý bộ nhớ an toàn hơn
    std::array<mpv_render_param, 4> params {{
        { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
        { PARAM_FRAMEBUFFER_SIZE, fb_size.data() },
        { PARAM_FLIP_Y, &flip },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    }};

    mpv_render_context_render(render_ctx, params.data());
}

void CleanupMPV() {
    if (render_ctx) {
        mpv_render_context_free(render_ctx);
        render_ctx = nullptr;
    }
    if (mpv) {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
}
