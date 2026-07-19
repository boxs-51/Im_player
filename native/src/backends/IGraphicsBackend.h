// IGraphicsBackend.h
#pragma once
#include <SDL.h>
#include <imgui.h>
#include <any>
class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;
    
    virtual Uint32 GetWindowFlags() = 0;
    virtual bool InitContext(SDL_Window* window) = 0;
    virtual bool InitImGuiBackend(SDL_Window* window) = 0;

    virtual std::any CreateSubContext() = 0; 
    virtual bool MakeCurrent(SDL_Window* window, const std::any& context) = 0;
    
    virtual void BeginFrame(SDL_Window* window) = 0;
    virtual void EndFrame(SDL_Window* window) = 0;
    virtual void Shutdown() = 0;
};