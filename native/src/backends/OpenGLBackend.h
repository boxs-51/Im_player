// OpenGLBackend.h
#pragma once
#include "IGraphicsBackend.h"
#include <gl3w.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include "player/render/OpenGLFrameBufferPool.h" // Thêm include
#include <imgui.h>
#include <imgui_internal.h>
#include <mpv/render_gl.h>

class OpenGLBackend : public IGraphicsBackend {
private:
    SDL_GLContext glContext = nullptr;
    // Các biến thành viên để lưu trữ dữ liệu cho mpv_render_param
    mpv_opengl_fbo m_mpv_fbo;
    int m_mpv_flip;
    // Các thuộc tính OpenGL có thể tùy chỉnh
    GLint internalFormat;
    GLenum format;
    GLenum type;

public:
    OpenGLBackend(GLint internalFormat = GL_RGBA8, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE)
        : internalFormat(internalFormat), format(format), type(type) {}

    Uint32 GetWindowFlags() override { return SDL_WINDOW_OPENGL; }

    std::vector<mpv_render_param> GetPlayBackRenderParams(const ImVec2& size) override {
        // Cập nhật các biến thành viên
        m_mpv_fbo = { 0, (int)size.x, (int)size.y, internalFormat };
        m_mpv_flip = 1;

        // Trả về vector các tham số
        return {
            { MPV_RENDER_PARAM_OPENGL_FBO, &m_mpv_fbo },
            { MPV_RENDER_PARAM_FLIP_Y, &m_mpv_flip },
            { MPV_RENDER_PARAM_INVALID, nullptr }
        };
    }

    const char* GetMpvApiType() const override{
        return "opengl";
    }
    unsigned int GetGLInternalFormat() const override {
        return internalFormat;
    }

    bool InitContext(SDL_Window* window) override {
        // Cấu hình chia sẻ context toàn cục trước khi tạo bất kỳ context nào
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        
        // Thiết lập profile và version OpenGL đồng bộ (Ví dụ: 4.3 Core)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

        glContext = SDL_GL_CreateContext(window);
        if (!glContext) return false;
        
        if (SDL_GL_MakeCurrent(window, glContext) != 0) return false;
        if (gl3wInit() != 0) return false;
        
        SDL_GL_SetSwapInterval(1);

        return true;
    }

    void Resize(int newWidth, int newHeight) override {
        glViewport(0, 0, newWidth, newHeight);
    }

    bool InitImGuiBackend(SDL_Window* window) override {
        ImGui_ImplSDL2_InitForOpenGL(window, glContext);
        ImGui_ImplOpenGL3_Init("#version 430 core");
        ImGuiIO& io = ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        return true;
    }

    void BeginFrame(SDL_Window* window) override {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Bạn có thể giữ hoặc bỏ clear tùy vào việc Renderer chính có clear nền riêng không
         glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
         glClear(GL_COLOR_BUFFER_BIT);
    }

    void EndFrame(SDL_Window* window) override {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void SwapWindow(SDL_Window* window) override {
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_context = SDL_GL_GetCurrentContext();

            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();

            SDL_GL_MakeCurrent(backup_current_window, backup_context);
        }
        SDL_GL_SwapWindow(window);
    }

    bool ProcessEvent(const SDL_Event* e) override {
        return ImGui_ImplSDL2_ProcessEvent(e);
    }


    void Shutdown(bool isFinalShutdown) override {
        if (isFinalShutdown) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL2_Shutdown();
        }
        if (glContext) SDL_GL_DeleteContext(glContext);
    }

    std::any CreateSubContext(SDL_Window* ownerWindow) override {
        if (!ownerWindow) return std::any();
        
        // Đảm bảo thuộc tính chia sẻ context luôn được bật trước khi ra lệnh tạo
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

        // 1. Tạo Sub Context (SDL2 sẽ tự động gán context mới này làm CURRENT của luồng hiện tại!)
        SDL_GLContext subContext = SDL_GL_CreateContext(ownerWindow);
        if (!subContext) {
            SDL_Log("Backend Error: Không thể tạo Shared GL Context!");
            return std::any();
        }
        
        // 2. BƯỚC QUAN TRỌNG: Unbind hoàn toàn luồng này về NULL để nhả tự do subContext ra hoàn toàn
        SDL_GL_MakeCurrent(ownerWindow, NULL); 
        
        // 3. Khôi phục lại context chính (glContext) để luồng UI chính tiếp tục làm việc bình thường
        SDL_GL_MakeCurrent(ownerWindow, glContext); 
        
        // Giờ đây subContext đang ở trạng thái tự do 100%, sẵn sàng cho luồng MPV chiếm quyền bằng MakeCurrent
        return std::make_any<SDL_GLContext>(subContext);
    }

    bool MakeCurrent(SDL_Window* window, const std::any& context) override {
        try {
            SDL_GLContext ctx = std::any_cast<SDL_GLContext>(context);
            // Nếu truyền vào std::any rỗng hoặc NULL context thì unbind
            return SDL_GL_MakeCurrent(window, ctx) == 0;
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    unsigned int GetGLFormat() const override {
        return format;
    }

    unsigned int GetGLType() const override {
        return type;
    }

    std::unique_ptr<IFrameBufferPool> CreateFrameBufferPool() override {
        return std::make_unique<OpenGLFrameBufferPool>();
    }
};