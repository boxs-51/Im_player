// IGraphicsBackend.h
#pragma once
#include <SDL.h>
#include <imgui.h>
#include <any>
#include <vector>
#include <mpv/render.h>
#include "mpv/render/IFrameBufferPool.h"
class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;
    virtual const char* GetMpvApiType() const = 0;
    
    virtual Uint32 GetWindowFlags() = 0;
    virtual bool InitContext(SDL_Window* window) = 0;
    virtual bool InitImGuiBackend(SDL_Window* window) = 0;

    virtual std::any CreateSubContext(SDL_Window* ownerWindow) = 0; 
    virtual bool MakeCurrent(SDL_Window* window, const std::any& context) = 0;
    
    virtual void BeginFrame(SDL_Window* window) = 0;
    virtual void EndFrame(SDL_Window* window) = 0; // Hàm mới để gọi ImGui::Render()
    virtual void SwapWindow(SDL_Window* window) = 0; // Hàm mới để hoán đổi buffer
    
    virtual bool ProcessEvent(const SDL_Event* e) = 0;
    // Cung cấp các tham số render cho mpv_render_context_render
    virtual std::vector<mpv_render_param> GetMpvRenderParams(const ImVec2& size) = 0;

    // Các getter cho thuộc tính OpenGL
    virtual unsigned int GetGLInternalFormat() const = 0;
    virtual unsigned int GetGLFormat() const = 0;
    virtual unsigned int GetGLType() const = 0;
    
    // Factory method để tạo FBO Pool tương ứng
    virtual std::unique_ptr<IFrameBufferPool> CreateFrameBufferPool() = 0;

    virtual void Shutdown(bool isFinalShutdown) = 0;
};