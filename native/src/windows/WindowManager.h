// WindowManager.h
#pragma once
#include <unordered_map>
#include <memory>
#include <SDL.h>
#include "WindowRuntime.h"
#include "WindowFactory.h"

class WindowManager {
private:
    WindowFactory* factory;
    std::unordered_map<WindowId, std::unique_ptr<WindowRuntime>> windows;

public:
    WindowManager(WindowFactory* fact) : factory(fact) {}

    WindowRuntime* CreateNewWindow(const std::string& templateName, std::unique_ptr<IGraphicsBackend> backend) {
        auto* runtime = factory->Create(templateName, std::move(backend));
        if (runtime) {
            windows[runtime->id] = std::unique_ptr<WindowRuntime>(runtime);
            return runtime;
        }
        return nullptr;
    }

    void DestroyWindow(WindowId id) {
        windows.erase(id);
    }

    // Hàm tiện ích mới: Tra cứu WindowRuntime từ SDL_Window vật lý
    WindowRuntime* GetWindowBySDLHandle(SDL_Window* sdlWin) {
        if (!sdlWin) return nullptr;
        for (auto& [id, runtime] : windows) {
            if (runtime->sdlWindow == sdlWin) {
                return runtime.get();
            }
        }
        return nullptr;
    }

    // Hỗ trợ vòng lặp range-based cho việc Duyệt Render ở main loop
    auto begin() { return windows.begin(); }
    auto end() { return windows.end(); }
};