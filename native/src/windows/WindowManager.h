// WindowManager.h
#pragma once
#include <unordered_map>
#include <memory>
#include <SDL.h>
#include "WindowRuntime.h"
#include "WindowFactory.h"
#include <mutex>
#include <functional>


/**
 * @brief 
 * 
 */
class WindowManager {
private:
    WindowFactory* factory;
    std::unordered_map<WindowId, std::unique_ptr<WindowRuntime>> windows;

    std::mutex m_queueMutex;
    std::vector<std::function<void()>> m_creationQueue;


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

    /**
     * @brief Create a New Window object
     * 
     * @param templateName 
     * @return WindowRuntime* 
     */
    // Trả về WindowId để có thể theo dõi cửa sổ vừa được yêu cầu tạo
    WindowId QueueCreateWindow(const std::string& templateName, WindowRuntime* parent = nullptr) {
        WindowId newId = 0; // Sẽ được gán trong factory
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_creationQueue.emplace_back([this, templateName, parent, &newId]() {
            auto* runtime = factory->Create(templateName, parent); // Factory sẽ gán ID
            if (runtime) {
                newId = runtime->info.id;
                windows[runtime->info.id] = std::unique_ptr<WindowRuntime>(runtime);

                // Thiết lập quan hệ hai chiều
                if (parent) {
                    runtime->relation.parent = parent;
                    parent->relation.AddChild(runtime);
                }
            }
        });
        return newId;
    }

    void ProcessCreationQueue() {
        std::vector<std::function<void()>> queueCopy;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            queueCopy.swap(m_creationQueue);
        }
        for (const auto& task : queueCopy) {
            task();
        }
    }

    /**
     * @brief 
     * 
     * @param id 
     */
    void DestroyWindow(WindowId id) {
        auto it = windows.find(id);
        if (it == windows.end()) return;

        WindowRuntime* runtime = it->second.get();

        // 1. Hủy tất cả các cửa sổ con của nó trước (đệ quy)
        // Tạo một bản sao của children vì vector gốc sẽ bị thay đổi trong vòng lặp
        std::vector<WindowRuntime*> childrenCopy = runtime->relation.children;
        for (WindowRuntime* child : childrenCopy) {
            DestroyWindow(child->info.id);
        }

        // 2. Xóa chính nó khỏi danh sách con của cha nó
        if (runtime->relation.parent) {
            runtime->relation.parent->relation.RemoveChild(runtime);
        }

        // 3. Cuối cùng, xóa chính nó
        windows.erase(it);
    }

    void HideWindow(WindowId id) {
        WindowRuntime* runtime = GetWindowById(id);
        if (runtime && runtime->resource.sdlWindow) {
            SDL_HideWindow(runtime->resource.sdlWindow);
    //        runtime->state.isShown = false;
    //        runtime->isTemporarilyHidden = true; // Đánh dấu là chỉ ẩn tạm thời
        }
    }

    void ShowWindow(WindowId id) {
        WindowRuntime* runtime = GetWindowById(id);
        if (runtime && runtime->resource.sdlWindow) {
            SDL_ShowWindow(runtime->resource.sdlWindow);
    //        runtime->state.isShown = true;
    //        runtime->isTemporarilyHidden = false;
        }
    }

    // Tìm một cửa sổ đã bị ẩn dựa trên template name
    WindowRuntime* FindHiddenWindowByTemplate(const std::string& templateName) {
        for (auto& [id, window] : windows) {
            if (window->info.templateName == templateName) {
                return window.get();
            }
        }
        return nullptr;
    }

    // Hàm tiện ích mới: Tra cứu WindowRuntime từ SDL_Window vật lý
    /**
     * @brief Get the Window By S D L Handle object
     * 
     * @param sdlWin 
     * @return WindowRuntime* 
     */
    WindowRuntime* GetWindowBySDLHandle(SDL_Window* sdlWin) {
        if (!sdlWin) return nullptr;
        for (auto& [id, runtime] : windows) {
            if (runtime->resource.sdlWindow == sdlWin) {
                return runtime.get();
            }
        }
        return nullptr;
    }
    /**
     * @brief Get the All Windows object
     * 
     * @return std::vector<WindowRuntime*> 
     */
    std::vector<WindowRuntime*> GetAllWindows() {
        std::vector<WindowRuntime*> result;
        result.reserve(windows.size());
        for (auto& [id, runtime] : windows) {
            result.push_back(runtime.get());
        }
        return result;
    }
    /**
     * @brief Get the Window By Id object
     * 
     * @param id 
     * @return WindowRuntime* 
     */
    WindowRuntime* GetWindowById(WindowId id) {
        auto it = windows.find(id);
        if (it != windows.end()) {
            return it->second.get();
        }
        return nullptr;
    }
    /**
     * @brief Get the Main Window object
     * 
     * @return WindowRuntime* 
     */
    WindowRuntime* GetMainWindow() {
        for (auto& [id, runtime] : windows) {
            if (runtime && runtime->style.isMainWindow) {
                return runtime.get();
            }
        }
        return nullptr; // Trả về nullptr nếu không tìm thấy cửa sổ chính nào
    }

    /**
     * @brief Lấy con trỏ WindowRuntime từ một sự kiện SDL.
     * 
     * @param e Sự kiện SDL.
     * @return WindowRuntime* Con trỏ đến runtime tương ứng, hoặc nullptr.
     */
    WindowRuntime* GetWindowFromEvent(const SDL_Event* e) {
        if (!e) return nullptr;

        Uint32 winID = 0;
        switch (e->type) {
            // --- Window Events ---
            case SDL_WINDOWEVENT:           winID = e->window.windowID; break;

            // --- Keyboard Events ---
            case SDL_KEYDOWN:               // Fallthrough
            case SDL_KEYUP:                 winID = e->key.windowID; break;
            case SDL_TEXTEDITING:           winID = e->edit.windowID; break;
            case SDL_TEXTINPUT:             winID = e->text.windowID; break;
            case SDL_KEYMAPCHANGED:         break; // Không có windowID

            // --- Mouse Events ---
            case SDL_MOUSEMOTION:           winID = e->motion.windowID; break;
            case SDL_MOUSEBUTTONDOWN:       // Fallthrough
            case SDL_MOUSEBUTTONUP:         winID = e->button.windowID; break;
            case SDL_MOUSEWHEEL:            winID = e->wheel.windowID; break;

            // --- Drag and Drop Events ---
            case SDL_DROPFILE:              // Fallthrough
            case SDL_DROPTEXT:              // Fallthrough
            case SDL_DROPBEGIN:             // Fallthrough
            case SDL_DROPCOMPLETE:          winID = e->drop.windowID; break;

            // --- Display Events ---
            //case SDL_DISPLAYEVENT:          winID = e->display.windowID; break;

            // --- Touch Events ---
            case SDL_FINGERDOWN:            // Fallthrough
            case SDL_FINGERUP:              // Fallthrough
            case SDL_FINGERMOTION:          winID = e->tfinger.windowID; break;

            // --- Gesture Events ---
            case SDL_DOLLARGESTURE:         // Fallthrough
            case SDL_DOLLARRECORD:          // Fallthrough
            //case SDL_MULTIGESTURE:          winID = e->mgesture.windowID; break;

            // --- Sensor Events ---
            //case SDL_SENSORUPDATE:          winID = e->sensor.windowID; break;

            // Các event còn lại (Joystick, Controller, Audio, User, Quit, v.v.)
            // không thuộc về một window cụ thể nên winID mặc định = 0.
            default:                        break;
        }

        if (winID == 0) return nullptr;

        SDL_Window* sdlWin = SDL_GetWindowFromID(winID);
        return GetWindowBySDLHandle(sdlWin);
    }

    /**
     * @brief Lấy con trỏ đến cửa sổ cha của một cửa sổ.
     * 
     * @param childId ID của cửa sổ con.
     * @return WindowRuntime* Con trỏ đến cửa sổ cha, hoặc nullptr.
     */
    WindowRuntime* GetParentOf(WindowId childId) {
        if (WindowRuntime* child = GetWindowById(childId)) {
            return child->relation.parent;
        }
        return nullptr;
    }

    /**
     * @brief Lấy danh sách con trỏ đến tất cả cửa sổ con của một cửa sổ.
     * 
     * @param parentId ID của cửa sổ cha.
     * @return std::vector<WindowRuntime*> Danh sách các cửa sổ con.
     */
    std::vector<WindowRuntime*> GetChildrenOf(WindowId parentId) {
        if (WindowRuntime* parent = GetWindowById(parentId)) {
            return parent->relation.children;
        }
        return {};
    }
    
    // Hỗ trợ vòng lặp range-based cho việc Duyệt Render ở main loop
    /**
     * @brief 
     * 
     * @return auto 
     */
    auto begin() { return windows.begin(); }
    /**
     * @brief 
     * 
     * @return auto 
     */
    auto end() { return windows.end(); }
};