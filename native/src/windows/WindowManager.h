// WindowManager.h
#pragma once
#include <unordered_map>
#include <memory>
#include "WindowRuntime.h"
#include "WindowFactory.h"

class WindowManager {
private:
    std::unordered_map<WindowId, std::unique_ptr<WindowRuntime>> windows;
    WindowFactory* factory;

public:
    WindowManager(WindowFactory* fact) : factory(fact) {}

    WindowRuntime* CreateNewWindow(const std::string& templateName) {
        WindowRuntime* runtime = factory->Create(templateName);
        if (runtime) {
            windows[runtime->id] = std::unique_ptr<WindowRuntime>(runtime);
            // Hiện thị cửa sổ sau khi đã chuẩn bị xong xuôi
            SDL_ShowWindow(runtime->sdlWindow);
        }
        return runtime;
    }

    WindowRuntime* GetWindow(WindowId id) {
        auto it = windows.find(id);
        return (it != windows.end()) ? it->second.get() : nullptr;
    }

    void DestroyWindow(WindowId id) {
        auto it = windows.find(id);
        if (it != windows.end()) {
            RemovePropW(it->second->hwnd, L"WINDOW_RUNTIME_PTR"); // Clean up WinAPI prop
            if (it->second->renderer) {
                it->second->renderer->Shutdown();
            }
            windows.erase(it);
        }
    }
    WindowRuntime* GetWindowBySDLHandle(SDL_Window* sdlWin) {
        if (!sdlWin) return nullptr;
        
        // Duyệt qua map hoặc vector lưu trữ các unique_ptr<WindowRuntime> nội bộ của bạn
        for (auto& [id, windowInstance] : windows) {
            if (windowInstance->sdlWindow == sdlWin) {
                return windowInstance.get();
            }
        }
        return nullptr;
    }

    // Hỗ trợ vòng lặp Enumerate duyệt qua mọi cửa sổ nhanh gọn
    auto begin() { return windows.begin(); }
    auto end() { return windows.end(); }
};