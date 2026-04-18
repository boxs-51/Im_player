// mpv_render_video.cpp
#include <mpv/mpv_render_video.h>
#include <mpv/mpv_data.h>
#include <mpv/shaders/shaders_manager.h>
#include <mpv/scripts/script_manager.h>
#include <mpv/fillter/audio_fillter_manager.h>

#include <windows/windows_borderless.h>
#include <threads/thread_manager.h>
#include <mpv/render_gl.h>

#include <string>
#include <optional>
#include <array>


// Thay vì #define, dùng constexpr để có type-safety
constexpr auto PARAM_FRAMEBUFFER_SIZE = static_cast<mpv_render_param_type>(3);
constexpr auto PARAM_FLIP_Y = static_cast<mpv_render_param_type>(4);

#ifdef RENDER_MPV_THREAD
MPVRenderThread renderThread;
#endif
#ifdef RENDER_MPV_FBO
MpvRender render;
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
    ShaderManager::Instance().Init(mpv_ptr);
    //AudioFilterManager::Instance().Init(mpv_ptr);

    ScriptManager::Instance().LoadScriptFromFolder({ AutoPath<std::string>("%ROOT%","scripts")});

    ShaderManager::Instance().LoadState();

    //AudioFilterManager::Instance().LoadFromFile();

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
#ifdef RENDER_MPV_FBO
void UpdateMPVTexture(int width, int height , MpvRender* rt) {
    if (width == rt->tex_width && height == rt->tex_height && rt->render_texture != 0) return;

    // Xóa cái cũ nếu đã tồn tại
    if (rt->m_fbo) glDeleteFramebuffers(1, &rt->m_fbo);
    if (rt->render_texture) glDeleteTextures(1, &rt->render_texture);

    rt->tex_width = width;
    rt->tex_height = height;

    // 1. Tạo Texture
    glGenTextures(1, &rt->render_texture);
    glBindTexture(GL_TEXTURE_2D, rt->render_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 2. Tạo FBO và gắn Texture vào
    glGenFramebuffers(1, &rt->m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->render_texture, 0);

    // Kiểm tra xem FBO có hợp lệ không
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Lỗi: Framebuffer không hợp lệ!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#endif
#ifdef RENDER_MPV_THREAD
GLuint GetStableFrameTexture(MPVRenderThread& rt) {
    static GLuint lastTex = 0;
    std::lock_guard<std::mutex> g(rt.swapMtx);

    if (rt.newFrameReady && rt.readIndex >= 0) {
        GLsync currentSync = rt.renderSyncs[rt.readIndex]; // Dùng mảng sync
        if (currentSync) {
            // Đợi tối đa 100ms
            GLenum waitReturn = glClientWaitSync(currentSync, GL_SYNC_FLUSH_COMMANDS_BIT, 100000000);
            
            if (waitReturn == GL_ALREADY_SIGNALED || waitReturn == GL_CONDITION_SATISFIED) {
                lastTex = rt.textures[rt.readIndex];
                rt.newFrameReady = false;
            } else if (waitReturn == GL_WAIT_FAILED) {
                // Có lỗi xảy ra với Context Sharing hoặc Driver
                // Trả về lastTex cũ để tránh nháy đen
            }
        } else {
            // Nếu chưa có sync (frame đầu tiên), cứ lấy texture luôn
            lastTex = rt.textures[rt.readIndex];
            rt.newFrameReady = false;
        }
    }
    return lastTex;
}
#endif
void RenderMPVVideo(const Vec2& size) {
    if (!mpv.render_ctx) return;
    #ifdef RENDER_MPV_FBO
    if (render.render_texture != 0) {
        // Hiển thị Texture lên giao diện ImGui
        ImGui::Image((ImTextureID)(intptr_t)render.render_texture, ToImVec2(size), ImVec2(0, 1), ImVec2(1, 0));
    }
    return;
    #endif
    #ifdef RENDER_MPV_THREAD
    GLuint tex = GetStableFrameTexture(renderThread);

    if (tex != 0) {

        ImGui::Image((ImTextureID)(intptr_t)tex, ToImVec2(size), ImVec2(0, 1), ImVec2(1, 0));
    }
    return;
    #endif
    #ifdef RENDER_MPV_FBO
    UpdateMPVTexture(static_cast<int>(size.x), static_cast<int>(size.y), &render);
    #endif
    // Cấu hình Framebuffer Object
    mpv_opengl_fbo fbo {};
    #ifdef RENDER_MPV_FBO
    fbo.fbo = render.m_fbo;
    #else 
    fbo.fbo = 0;
    #endif
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

    mpv_render_context_render(mpv.render_ctx, params.data());


}
#ifdef RENDER_MPV_THREAD
void InitRenderFBO(MPVRenderThread* rt) {
    glGenFramebuffers(3, rt->fbos);
    glGenTextures(3, rt->textures);

    for (int i = 0; i < 3; i++) {
        glBindTexture(GL_TEXTURE_2D, rt->textures[i]);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     rt->width, rt->height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbos[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            rt->textures[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("FBO[%d] not complete\n", i);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MPVRenderLoop(MPVRenderThread* rt) {

    SDL_GL_MakeCurrent(rt->window, rt->glContext);

    InitRenderFBO(rt);
    FrameTimer framerender(30);
    while (rt->running) {
        std::unique_lock lock(rt->mtx);

        rt->cv.wait(lock, [&] {
            return rt->needRender || !rt->running;
        });

        if (!rt->running) break;

        rt->needRender = false;
        
        if (rt->needResize) {
            rt->width = rt->newW;
            rt->height = rt->newH;

            // Đánh dấu cả 3 textures là "đã lỗi thời, cần resize"
            rt->dirtyTextures[0] = true;
            rt->dirtyTextures[1] = true;
            rt->dirtyTextures[2] = true;

            rt->needResize = false;
        }

        lock.unlock();
        int index = rt->writeIndex;

        if (rt->dirtyTextures[index]) {
            glBindTexture(GL_TEXTURE_2D, rt->textures[index]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rt->width, rt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            // Gắn lại vào FBO
            glBindFramebuffer(GL_FRAMEBUFFER, rt->fbos[index]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->textures[index], 0);

            if (rt->renderSyncs[index]) {
                glDeleteSync(rt->renderSyncs[index]);
                rt->renderSyncs[index] = nullptr;
            }
            
            // Đã tạo mới xong, tắt cờ đi
            rt->dirtyTextures[index] = false; 
        }

        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbos[index]);

        glViewport(0, 0, rt->width, rt->height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Xóa màu đen
        glClear(GL_COLOR_BUFFER_BIT); // Hoặc chỉ glClear(GL_COLOR_BUFFER_BIT)

        mpv_opengl_fbo fbo {};
        fbo.fbo = rt->fbos[index];
        fbo.w = rt->width;
        fbo.h = rt->height;
        fbo.internal_format = GL_RGBA8;

        int flip = 1;
        std::array<int, 2> fb_size = { static_cast<int>(rt->width), static_cast<int>(rt->height) };

        // Tổ chức params bằng std::array để quản lý bộ nhớ an toàn hơn
        std::array<mpv_render_param, 4> params {{
            { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
            { PARAM_FRAMEBUFFER_SIZE, fb_size.data() },
            { PARAM_FLIP_Y, &flip },
            { MPV_RENDER_PARAM_INVALID, nullptr }
        }};

        mpv_render_context_render(rt->ctx, params.data());
        if (rt->renderSyncs[index]) {
            glDeleteSync(rt->renderSyncs[index]);
        }
        //glBindFramebuffer(GL_FRAMEBUFFER, 0);
        rt->renderSyncs[index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();  // Hoặc glFinish() nếu vẫn bị nháy hình
        {
            std::lock_guard<std::mutex> g(rt->swapMtx);

            rt->readIndex = index;
            rt->writeIndex = (rt->writeIndex + 1) % 3;
            rt->newFrameReady = true;
            
        }
        rt->framerender.store(framerender.updateAndGetFPS());
        SDL_Event ev;
        //SDL_ZeroObject(ev);
        ev.type = SDL_MPV_RENDER_UPDATE;
        SDLUtils::SDLX_PushUniqueEvent(ev);
    }
    glDeleteFramebuffers(3, rt->fbos);
    glDeleteTextures(3, rt->textures);
    
    for(int i=0; i<3; i++) {
        if(rt->renderSyncs[i]) glDeleteSync(rt->renderSyncs[i]);
    }
    
    SDL_GL_DeleteContext(rt->glContext);
    rt->hasExited = true;
}

void StartMPVRenderThread() {
   
    renderThread.ctx = mpv.render_ctx;
    renderThread.window = ctx.mainWindow;

    renderThread.width = (int)Windowlayout.VideoSize.x;
    renderThread.height = (int)Windowlayout.VideoSize.y;
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

    // 5. Giải phóng tài nguyên OpenGL (Phải chạy trong context đã tạo ra chúng)
    // Lưu ý: glDeleteSync, glDeleteTextures, glDeleteFramebuffers 
    // nên được gọi TRƯỚC khi SDL_GL_DeleteContext bị gọi.
    #ifdef RENDER_MPV_THREAD

    for(int i=0; i<3; i++) {
        if(renderThread.renderSyncs[i]) glDeleteSync(renderThread.renderSyncs[i]);
    }
    
    #endif
}
