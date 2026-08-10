// WindowResource.h
#pragma once
#include <SDL.h>
#include <windows.h>
#include <memory>
#include "imgui.h"

#include "player/session/PlayerManager.h"

// Forward declarations
class IGraphicsBackend;
class UIRenderThread;
class PlayerSession;
class WindowSharedGroup;
class PlayerManager;

struct WindowResource {
    SDL_Window* sdlWindow = nullptr;
    HWND hwnd = nullptr;
    ImGuiContext* imguiCtx = nullptr;
    std::unique_ptr<IGraphicsBackend> graphicsBackend;
    std::unique_ptr<UIRenderThread> uiRenderThread;

    std::string playersessionid; // ID phiên phát chính thức
    PlayerSession* playersession = nullptr; // Non-owning pointer (Dùng tạm thời / Backward compatibility)

    std::shared_ptr<WindowSharedGroup> sharedGroup; // Chỉ root window mới sở hữu

    // Tra cứu an toàn qua PlayerManager, chống Dangling Pointer / Use-After-Free
    PlayerSession* GetPlayerSession() const {
        if (!playersessionid.empty()) {
            return PlayerManager::GetInstance().GetSession(playersessionid);
        }
        return playersession; // Fallback nếu chưa gán ID
    }
};