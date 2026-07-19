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
    WindowManager() = default;

public:
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    // BƯỚC 3: Hàm tĩnh để lấy Instance duy nhất (Thread-safe từ C++11)
    static WindowManager& GetInstance() {
        static WindowManager instance;
        return instance;
    }
    void Initialize(WindowFactory* fact) {
        factory = fact;
    }

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
    std::vector<WindowRuntime*> GetAllWindows() {
        std::vector<WindowRuntime*> result;
        result.reserve(windows.size());
        for (auto& [id, runtime] : windows) {
            result.push_back(runtime.get());
        }
        return result;
    }
    WindowRuntime* GetWindowById(WindowId id) {
        auto it = windows.find(id);
        if (it != windows.end()) {
            return it->second.get();
        }
        return nullptr;
    }
    WindowRuntime* GetMainWindow() {
        for (auto& [id, runtime] : windows) {
            if (runtime && runtime->style.isMainWindow) {
                return runtime.get();
            }
        }
        return nullptr; // Trả về nullptr nếu không tìm thấy cửa sổ chính nào
    }
    
    // Hỗ trợ vòng lặp range-based cho việc Duyệt Render ở main loop
    auto begin() { return windows.begin(); }
    auto end() { return windows.end(); }
};