// mpv_render_video.cpp
#include <mpv/mpv_render_video.h>
#include <mpv/mpv_data.h>
#include <mpv/shaders/shaders_manager.h>
#include <mpv/scripts/script_manager.h>
#include <mpv/audio/filter/af_m.h>

#include <windows/windows_borderless.h>
#include "MainWindowState.h"
#include <threads/thread_manager.h>
#include <mpv/render_gl.h>
#include <gl3w.h> // Sử dụng để giữ lại các lệnh cấp phát Texture/FBO đặc thù của OpenGL

#include <string>
#include <optional>
#include <array>

#ifdef RENDER_MPV_THREAD
MPVRenderThread renderThread;
#endif

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

    ScriptManager::Instance().Init(mpv_ptr);
    ScriptManager::Instance().LoadScriptFromFolder({ AutoPath<std::string>("%ROOT%","scripts")});

    ShaderManager::Instance().Init(mpv_ptr);
    ShaderManager::Instance().LoadState();

    AudioFilterManager::Instance().Init(mpv_ptr);
    AudioFilterManager::Instance().LoadFromFile();

    mpv_request_log_messages(mpv_ptr, "v");
    InitMPVObservers(mpv_ptr);
    
    return true;
}

bool InitMPVRenderContext(mpv_handle* mpv_ptr) {
    if (!mpv_ptr) return false;

    mpv_opengl_init_params gl_init_params {};
    gl_init_params.get_proc_address = GetProcAddressWrapper;
    gl_init_params.get_proc_address_ctx = nullptr;

    std::array<mpv_render_param, 3> render_params {{
        { MPV_RENDER_PARAM_API_TYPE, const_cast<char*>("opengl") },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    }};

    if (mpv_render_context_create(&mpv.render_ctx, mpv_ptr, render_params.data()) < 0)
        return false;

    #ifdef RENDER_MPV_THREAD
    mpv_render_context_set_update_callback(mpv.render_ctx,
    [](void* userdata) {
        auto* rt = (MPVRenderThread*)userdata;
        {
            std::lock_guard lock(rt->mtx);
            rt->needRender = true;
        }
        rt->cv.notify_one();
    }, &renderThread);
    #else
    mpv_render_context_set_update_callback(mpv.render_ctx, [](void*) {
        SDL_Event ev;
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev);
    }, nullptr);
    #endif

    mpv_set_wakeup_callback(mpv_ptr, [](void*) {
        SDL_Event ev;
        ev.type = SDL_MPV_EVENT;
        SDLUtils::SDLX_PushUniqueEvent(ev);
    }, nullptr);

    return true;
}

#ifdef RENDER_MPV_THREAD
FrameTextureInfo GetStableFrameTexture(MPVRenderThread& rt) {
    static int currentDisplayIndex = -1;
    int newReadyIndex = -1;

    for (int i = 0; i < 3; ++i) {
        BufferState expected = BufferState::READY;
        if (rt.frames[i].state.compare_exchange_strong(expected, BufferState::DISPLAYING, std::memory_order_acq_rel)) {
            newReadyIndex = i;
            break; 
        }
    }

    if (newReadyIndex != -1) {
        if (rt.frames[newReadyIndex].fence) {
            GLenum wait = glClientWaitSync(rt.frames[newReadyIndex].fence, GL_SYNC_FLUSH_COMMANDS_BIT, 16000000);
            if (wait == GL_WAIT_FAILED || wait == GL_TIMEOUT_EXPIRED) {
                rt.frames[newReadyIndex].state.store(BufferState::READY);
                newReadyIndex = -1; 
            }
        }
        
        if (newReadyIndex != -1) {
            if (currentDisplayIndex != -1 && currentDisplayIndex != newReadyIndex) {
                rt.frames[currentDisplayIndex].state.store(BufferState::FREE, std::memory_order_release);
            }
            currentDisplayIndex = newReadyIndex;
        }
    }

    FrameTextureInfo info;
    if (currentDisplayIndex != -1) {
        FrameNode& frame = rt.frames[currentDisplayIndex];
        info.texID = frame.texture;
        
        if (frame.allocatedW > 0 && frame.allocatedH > 0) {
            info.u = (float)frame.contentW / frame.allocatedW;
            info.v = (float)frame.contentH / frame.allocatedH;
        }
    }
    return info;
}
#endif

void RenderMPVVideo(const ImVec2& pos, const ImVec2& size) {
    if (!mpv.render_ctx || Audio_visualizers) return;

    #ifdef RENDER_MPV_THREAD
    FrameTextureInfo frameInfo = GetStableFrameTexture(renderThread);

    if (frameInfo.texID != 0) {
        ImGui::Image(
            (ImTextureID)(intptr_t)frameInfo.texID, 
            size, 
            ImVec2(0, 0), 
            ImVec2(frameInfo.u, frameInfo.v)
        );
    }
    return;
    #endif

    mpv_opengl_fbo fbo {};
    fbo.fbo = 0;
    fbo.w = static_cast<int>(size.x);
    fbo.h = static_cast<int>(size.y);
    fbo.internal_format = GL_RGBA8;

    int flip = 1; 
    int skip_render = (Audio_visualizers) ? 1 : 0;

    std::array<mpv_render_param, 4> params {{
        { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
        { MPV_RENDER_PARAM_FLIP_Y, &flip },
        { MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    }};

    mpv_render_context_render(mpv.render_ctx, params.data());
}

#ifdef RENDER_MPV_THREAD
void InitRenderFBO(MPVRenderThread* rt) {
    for (int i = 0; i < 3; i++) {
        glGenFramebuffers(1, &rt->frames[i].fbo);
        glGenTextures(1, &rt->frames[i].texture);

        glBindTexture(GL_TEXTURE_2D, rt->frames[i].texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 
                     rt->MAX_W, rt->MAX_H, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, rt->frames[i].fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->frames[i].texture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("FBO[%d] not complete\n", i);
        }
        rt->frames[i].allocatedW = rt->MAX_W;
        rt->frames[i].allocatedH = rt->MAX_H;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static int AcquireFreeBuffer(MPVRenderThread* rt) {
    for (int i = 0; i < 3; ++i) {
        BufferState expected = BufferState::FREE;
        if (rt->frames[i].state.compare_exchange_strong(expected, BufferState::RENDERING, std::memory_order_acq_rel)) {
            return i;
        }
    }
    for (int i = 0; i < 3; ++i) {
        BufferState expected = BufferState::READY;
        if (rt->frames[i].state.compare_exchange_strong(expected, BufferState::RENDERING, std::memory_order_acq_rel)) {
            return i;
        }
    }
    return 0;
}

void MPVRenderLoop(MPVRenderThread* rt, WindowRuntime* runtime) {
    if (!runtime || !runtime->graphicsBackend) {
        rt->hasExited = true;
        return;
    }

    // Kích hoạt Shared Sub-Context riêng của MPV lên luồng này thông qua Backend trừu tượng
    if (!runtime->graphicsBackend->MakeCurrent(rt->window, rt->graphicsContext)) {
        SDL_Log("Lỗi: Không thể bind MPV Sub-Context lên Render Thread!");
        rt->hasExited = true;
        return;
    }
    SDL_GL_SetSwapInterval(1);
    InitRenderFBO(rt);
    FrameTimer framerender(30);

    for(int i = 0; i < 3; ++i) {
        rt->frames[i].state.store(BufferState::FREE);
    }

    while (rt->running) {
        std::unique_lock lock(rt->mtx);
        rt->cv.wait(lock, [&] {
            return rt->needRender || !rt->running;
        });

        if (!rt->running) break;

        rt->needRender = false;
        
        if (rt->surface.needResize) {
            rt->surface.drawW = rt->surface.newW;
            rt->surface.drawH = rt->surface.newH;
            rt->surface.needResize = false;
        }

        lock.unlock();

        bool isVisible = rt->g_WindowVisible.load(std::memory_order_relaxed);
        bool isVisualizer = rt->Audio_visualizers.load(std::memory_order_relaxed);
        bool isZeroSize = (rt->surface.drawW <= 0 || rt->surface.drawH <= 0);
        int skip_render = (!isVisible || isVisualizer || isZeroSize) ? 1 : 0;

        if (skip_render) {
            std::array<mpv_render_param, 2> skip_params {{
                { MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render },
                { MPV_RENDER_PARAM_INVALID, nullptr }
            }};
            mpv_render_context_render(rt->ctx, skip_params.data());
            continue; 
        }

        int index = AcquireFreeBuffer(rt);
        FrameNode& frame = rt->frames[index];

        int targetW = rt->surface.drawW;
        int targetH = rt->surface.drawH;
        bool needsRealloc = false;

        if (targetW > frame.allocatedW || targetH > frame.allocatedH) {
            int newW = std::min(rt->MAX_SAFE_TEXTURE_SIZE, (int)(frame.allocatedW * 1.5f));
            int newH = std::min(rt->MAX_SAFE_TEXTURE_SIZE, (int)(frame.allocatedH * 1.5f));

            frame.allocatedW = std::max(targetW, newW);
            frame.allocatedH = std::max(targetH, newH);
            frame.allocatedW = std::min(frame.allocatedW, rt->MAX_SAFE_TEXTURE_SIZE);
            frame.allocatedH = std::min(frame.allocatedH, rt->MAX_SAFE_TEXTURE_SIZE);
            needsRealloc = true;
        }

        frame.contentW = std::min(targetW, frame.allocatedW);
        frame.contentH = std::min(targetH, frame.allocatedH);

        if (needsRealloc && frame.allocatedW > 0 && frame.allocatedH > 0) {
            glBindTexture(GL_TEXTURE_2D, frame.texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame.allocatedW, frame.allocatedH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            
            glBindFramebuffer(GL_FRAMEBUFFER, frame.fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame.texture, 0);
        }

        mpv_opengl_fbo fbo {};
        fbo.fbo = frame.fbo;
        fbo.w = frame.contentW;
        fbo.h = frame.contentH;
        fbo.internal_format = GL_RGBA8;

        int flip = 0;

        std::array<mpv_render_param, 4> params {{
            { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
            { MPV_RENDER_PARAM_FLIP_Y, &flip },
            { MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render },
            { MPV_RENDER_PARAM_INVALID, nullptr }
        }};

        mpv_render_context_render(rt->ctx, params.data());

        if (frame.fence) glDeleteSync(frame.fence);

        frame.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();  
        frame.state.store(BufferState::READY, std::memory_order_release);

        rt->framerender.store(framerender.updateAndGetFPS());

        SDL_Event ev;
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    for(int i = 0; i < 3; ++i) {
        glDeleteFramebuffers(1, &rt->frames[i].fbo);
        glDeleteTextures(1, &rt->frames[i].texture);
    }

    // Đoạn code hủy context thô cũ đã được loại bỏ, nó sẽ giải phóng tự động khi kết thúc App hoặc do Backend quản lý.
    rt->hasExited = true;
}

void StartMPVRenderThread(WindowRuntime* runtime) {
    if (!runtime || !mpv.render_ctx || !runtime->graphicsBackend) return;

    renderThread.ownerWindowId = runtime->id;
    renderThread.window = runtime->sdlWindow; 
    renderThread.running = true;
    renderThread.ctx = mpv.render_ctx;

    if (runtime->properties.Contains("Layout")) {
        auto layout = runtime->properties.Get<MainWindowLayout>("Layout");
        renderThread.surface.drawW = (int)layout.VideoSize.x;
        renderThread.surface.drawH = (int)layout.VideoSize.y;
    }

    // 1. Khởi tạo Sub-Context đồ họa an toàn và lưu vào Thread State
    std::any m_subContext = runtime->graphicsBackend->CreateSubContext();
    if (!m_subContext.has_value()) {
        SDL_Log("Lỗi: Không thể tạo Shared Context cho luồng phụ MPV!");
        return;
    }
    renderThread.graphicsContext = m_subContext;

    // 2. Kích hoạt luồng thông qua ThreadManager
    GetThreadManager().Run(ThreadID::MPVRenderThread, [runtime]() {
        MPVRenderLoop(&renderThread, runtime);
    });
    //runtime->graphicsBackend->MakeCurrent(runtime->sdlWindow, renderThread.graphicsContext);
}
#endif

void CleanupMPV() {
    #ifdef RENDER_MPV_THREAD
    renderThread.running = false;
    renderThread.cv.notify_all(); 

    while (GetThreadManager().IsRunning(ThreadID::MPVRenderThread)) {
        SDL_Delay(1);
    }
    #endif

    if (mpv.render_ctx) {
        mpv_render_context_free(mpv.render_ctx);
        mpv.render_ctx = nullptr;
    }

    if (mpv.mpv) {
        mpv_terminate_destroy(mpv.mpv);
        mpv.mpv = nullptr;
    }
}