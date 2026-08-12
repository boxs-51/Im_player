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
    std::shared_ptr<WindowSharedGroup> sharedGroup; // Chỉ root window mới sở hữu

    /**
     * @brief Lấy PlayerSession an toàn từ PlayerManager theo ID.
     * @return PlayerSession* Trả về pointer hợp lệ hoặc nullptr nếu Session đã bị hủy.
     */
    PlayerSession* GetPlayerSession() const {
        if (playersessionid.empty()) {
            return nullptr;
        }
        // Tra cứu trực tiếp từ PlayerManager. Nếu Session không còn tồn tại, tự động trả về nullptr an toàn
        return PlayerManager::GetInstance().GetSession(playersessionid);
    }
};