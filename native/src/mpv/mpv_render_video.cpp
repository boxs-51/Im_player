// mpv_render_video.cpp
#include <mpv/mpv_render_video.h>
#include <mpv/mpv_data.h>
#include <mpv/shaders/shaders_manager.h>
#include <mpv/scripts/script_manager.h>
#include <mpv/audio/filter/audio_filter_manager.h>

#include <windows/windows_borderless.h>
#include <threads/thread_manager.h>
#include <mpv/render_gl.h>

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

    if (mpv_render_context_create(&mpv.render_ctx, mpv_ptr, render_params.data()) < 0)
        return false;

    // Sử dụng Lambda capture nếu cần, nhưng ở đây dùng static callback của MPV
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
        //SDL_ZeroObject(ev);
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev);

    }, nullptr);
    #endif
    mpv_set_wakeup_callback(mpv_ptr, [](void*) {
        SDL_Event ev;
        //SDL_ZeroObject(ev);
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
                newReadyIndex = -1; // Fallback về frame cũ
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
        
        // Tính toán UV tự động dựa trên vùng nhớ dư thừa
        if (frame.allocatedW > 0 && frame.allocatedH > 0) {
            info.u = (float)frame.contentW / frame.allocatedW;
            info.v = (float)frame.contentH / frame.allocatedH;
        }
    }
    return info;
}
#endif
void RenderMPVVideo(const ImVec2& pos, const ImVec2& size) {
    if (!mpv.render_ctx || Audio_visualizers || !g_WindowVisible) return;

    //ImGui::SetNextWindowPos(pos);
    //ImGui::BeginChild("##Video", size);

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
    //ImGui::EndChild();
    return;
    #endif
    // Cấu hình Framebuffer Object
    mpv_opengl_fbo fbo {};
    fbo.fbo = 0;
    fbo.w = static_cast<int>(size.x);
    fbo.h = static_cast<int>(size.y);
    fbo.internal_format = GL_RGBA8;

    int flip = 1; 
    int skip_render = (!g_WindowVisible || Audio_visualizers) ? 1 : 0;

    // Tổ chức params bằng std::array để quản lý bộ nhớ an toàn hơn
    std::array<mpv_render_param, 4> params {{
        { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
        { MPV_RENDER_PARAM_FLIP_Y, &flip },
        { MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    }};

    mpv_render_context_render(mpv.render_ctx, params.data());

    //ImGui::EndChild();
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
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            rt->frames[i].texture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("FBO[%d] not complete\n", i);
        }
        rt->frames[i].allocatedW = rt->MAX_W;
        rt->frames[i].allocatedH = rt->MAX_H;

    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
static int AcquireFreeBuffer(MPVRenderThread* rt) {
    // Ưu tiên 1: Lấy buffer đang trống (FREE)
    for (int i = 0; i < 3; ++i) {
        BufferState expected = BufferState::FREE;
        if (rt->frames[i].state.compare_exchange_strong(expected, BufferState::RENDERING, std::memory_order_acq_rel)) {
            return i;
        }
    }

    // Ưu tiên 2: Nếu không có FREE (UI chậm hơn Render), lấy buffer READY để ghi đè
    for (int i = 0; i < 3; ++i) {
        BufferState expected = BufferState::READY;
        if (rt->frames[i].state.compare_exchange_strong(expected, BufferState::RENDERING, std::memory_order_acq_rel)) {
            return i;
        }
    }

    // Tình huống xấu nhất: Mọi thứ đang kẹt (Rất hiếm khi xảy ra ở Triple Buffer). 
    // Trả về đại 0 để không crash hệ thống.
    return 0;
}
void MPVRenderLoop(MPVRenderThread* rt) {

    SDL_GL_MakeCurrent(rt->window, rt->glContext);

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

        framerender.startFrame();

        rt->needRender = false;
        
        if (rt->surface.needResize) {
            rt->surface.drawW = rt->surface.newW;
            rt->surface.drawH = rt->surface.newH;
            rt->surface.needResize = false;
        }

        lock.unlock();

        bool isZeroSize = (rt->surface.drawW <= 0 || rt->surface.drawH <= 0);
        int skip_render = (rt->Audio_visualizers || !(rt->g_WindowVisible) || isZeroSize) ? 1 : 0;

        if (skip_render) {
            // Chỉ gọi mpv với 1 param duy nhất để nó tiếp tục xử lý audio/logic
            std::array<mpv_render_param, 2> skip_params {{
                { MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render },
                { MPV_RENDER_PARAM_INVALID, nullptr }
            }};
            mpv_render_context_render(rt->ctx, skip_params.data());
            
            // BỎ QUA HOÀN TOÀN việc lấy Buffer, Bind FBO, Sync Fence và Push Event
            // Tiết kiệm ~95% CPU/GPU overhead cho luồng này khi app bị ẩn
            continue; 
        }

        int index = AcquireFreeBuffer(rt);
        FrameNode& frame = rt->frames[index];

        int targetW = rt->surface.drawW;
        int targetH = rt->surface.drawH;
        bool needsRealloc = false;

        // Logic cấp phát lại động: Chỉ khi kích thước yêu cầu lớn hơn sức chứa hiện tại
        if (targetW > frame.allocatedW || targetH > frame.allocatedH) {
            // Nhân 1.5 để dự phòng: Nếu người dùng kéo cửa sổ từ 1080p lên 1440p, 
            // ta cấp phát thẳng dư ra một chút để lần sau kéo tiếp không bị khựng.

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

        // Thực hiện cấp phát bộ nhớ mới trên VRAM
        if (needsRealloc && frame.allocatedW > 0 && frame.allocatedH > 0) {
            glBindTexture(GL_TEXTURE_2D, frame.texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame.allocatedW, frame.allocatedH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            
            // Gắn lại texture vào FBO vì backing store đã thay đổi
            glBindFramebuffer(GL_FRAMEBUFFER, frame.fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame.texture, 0);
            
            //glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            //glClear(GL_COLOR_BUFFER_BIT);
        }else{
            //glBindFramebuffer(GL_FRAMEBUFFER, frame.fbo);
        }

        mpv_opengl_fbo fbo {};
        fbo.fbo = frame.fbo;
        fbo.w = frame.contentW;
        fbo.h = frame.contentH;
        fbo.internal_format = GL_RGBA8;

        int flip = 0;

        // Tổ chức params bằng std::array để quản lý bộ nhớ an toàn hơn
        std::array<mpv_render_param, 4> params {{
            { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
            { MPV_RENDER_PARAM_FLIP_Y, &flip },
            { MPV_RENDER_PARAM_SKIP_RENDERING, &skip_render },
            { MPV_RENDER_PARAM_INVALID, nullptr }
        }};

        mpv_render_context_render(rt->ctx, params.data());

        if (frame.fence) glDeleteSync(frame.fence);

        frame.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        //glFlush();  // Hoặc glFinish() nếu vẫn bị nháy hình
        //glFinish();
        frame.state.store(BufferState::READY, std::memory_order_release);

        rt->framerender.store(framerender.updateAndGetFPS());

        SDL_Event ev;
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev);

        framerender.endFrame();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    for(int i = 0; i < 3; ++i) {
        glDeleteFramebuffers(1, &rt->frames[i].fbo);
        glDeleteTextures(1, &rt->frames[i].texture);
    }

    SDL_GL_DeleteContext(rt->glContext);
    rt->hasExited = true;
}

void StartMPVRenderThread() {
   
    renderThread.ctx = mpv.render_ctx;
    renderThread.window = ctx.mainWindow;

    renderThread.surface.drawW = (int)Windowlayout.VideoSize.x;
    renderThread.surface.drawH = (int)Windowlayout.VideoSize.y;
    //SDL_GL_MakeCurrent(renderThread.window, NULL);
    renderThread.glContext = SDL_GL_CreateContext(renderThread.window);

    GetThreadManager().Run(ThreadID::MPVRenderThread, [&]() {
        MPVRenderLoop(&renderThread);
    });

    SDL_GL_MakeCurrent(ctx.mainWindow, ctx.mainGLContext);
}
#endif
void CleanupMPV() {
    #ifdef RENDER_MPV_THREAD
    // 1. Phát lệnh dừng luồng trước
    renderThread.running = false;
    renderThread.cv.notify_all(); 

    // 2. Phải đợi luồng kết thúc (nếu bạn lưu std::thread)
    // Nếu bạn dùng .detach() như code trước, bạn cần một flag để xác nhận luồng đã thoát
    // Ví dụ: while(!renderThread.hasExited) SDL_Delay(1);
    while (GetThreadManager().IsRunning(ThreadID::MPVRenderThread)) SDL_Delay(1);
    #endif
    // 3. Hủy Render Context của MPV (Phải gọi khi luồng render đã dừng)
    if (mpv.render_ctx) {
        mpv_render_context_free(mpv.render_ctx);
        mpv.render_ctx = nullptr;
    }

    // 4. Hủy MPV Handle
    if (mpv.mpv) {
        mpv_terminate_destroy(mpv.mpv);
        mpv.mpv = nullptr;
    }
    
}
