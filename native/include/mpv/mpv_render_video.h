#pragma once

#include <mpv/client.h>
#include <imgui.h>
#include <GL/gl3w.h>
#define SDL_MPV_EVENT (SDL_USEREVENT + 1)
#define SDL_MPV_RENDER_UPDATE (SDL_USEREVENT + 2)
#define SDL_MPV_RENDER_UPDATE_SYNC (SDL_USEREVENT + 3)

#ifdef RENDER_MPV_THREAD
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <SDL.h>
#include <mpv/render_gl.h>
struct MPVRenderThread {
    mpv_render_context* ctx = nullptr;

    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;

    GLuint fbos[3] = {};
    GLuint textures[3] = {};

    int writeIndex = 0;
    int readIndex = -1;

    std::mutex swapMtx;
    std::atomic<bool> newFrameReady = false;
    
    int width = 1280;
    int height = 720;

    std::mutex mtx;
    std::condition_variable cv;

    bool needRender = false;
    bool running = true;

    std::atomic<bool> needResize = false;
    std::atomic<int> newW, newH;

};
extern MPVRenderThread renderThread;
void StartMPVRenderThread();
#endif
#ifdef RENDER_MPV_FBO
struct MpvRender{
    GLuint render_texture = 0;
    GLuint m_fbo = 0;
    int tex_width = 0, tex_height = 0;
};
extern MpvRender render;
#endif
/// Khởi tạo mpv và thiết lập các tuỳ chọn cơ bản
bool InitMPV(mpv_handle*& mpv);

/// Tạo render context OpenGL cho mpv (render_ctx)
bool InitMPVRenderContext(mpv_handle* mpv);

/// Render video mpv ra FBO đang được ImGui/OpenGL sử dụng
void RenderMPVVideo(const ImVec2& size);

/// Dọn dẹp mpv + render context khi thoát
void CleanupMPV();
