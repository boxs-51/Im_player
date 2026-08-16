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
class WindowManager
{
private:
    WindowFactory *factory;
    std::unordered_map<WindowId, std::unique_ptr<WindowRuntime>> windows;
    mutable std::mutex m_windowsMutex;

    std::mutex m_queueMutex;
    std::vector<std::function<void()>> m_creationQueue;

    WindowManager() = default;

public:
    WindowManager(const WindowManager &) = delete;
    WindowManager &operator=(const WindowManager &) = delete;

    // BƯỚC 3: Hàm tĩnh để lấy Instance duy nhất (Thread-safe từ C++11)
    static WindowManager &GetInstance()
    {
        static WindowManager instance;
        return instance;
    }

    void Initialize(WindowFactory *fact)
    {
        factory = fact;
    }

    /**
     * @brief Tạo cửa sổ đồng bộ ngay trên thread hiện tại (dùng khi cần nhận ngay kết quả như Main Window)
     * @return WindowRuntime*
     */
    WindowRuntime *CreateWindowSync(const std::string &templateName, WindowRuntime *parent = nullptr)
    {
        if (!factory)
            return nullptr;

        auto *runtime = factory->Create(templateName, parent);
        if (runtime)
        {
            WindowId id = runtime->info.id;
            std::lock_guard<std::mutex> lock(m_windowsMutex);
            windows[id] = std::unique_ptr<WindowRuntime>(runtime);

            if (parent)
            {
                runtime->relation.parent = parent;
                parent->relation.AddChild(runtime);
            }
        }
        return runtime;
    }

    /**
     * @brief Đưa yêu cầu tạo cửa sổ vào hàng đợi (An toàn Thread)
     *
     * @param onCreated Callback tùy chọn được gọi sau khi cửa sổ tạo thành công kèm WindowId thật.
     * @param templateName
     * @return WindowRuntime*
     */
    void QueueCreateWindow(
        const std::string &templateName,
        WindowRuntime *parent = nullptr,
        std::function<void(WindowId)> onCreated = nullptr)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_creationQueue.emplace_back([this, templateName, parent, onCreated]()
                                     {
            if (!factory) return;

            auto* runtime = factory->Create(templateName, parent);
            if (runtime) {
                WindowId newId = runtime->info.id;
                {
                    std::lock_guard<std::mutex> lock(m_windowsMutex);
                    windows[newId] = std::unique_ptr<WindowRuntime>(runtime);
                }

                if (parent) {
                    runtime->relation.parent = parent;
                    parent->relation.AddChild(runtime);
                }

                if (onCreated) {
                    onCreated(newId);
                }
            } });
    }

    void ProcessCreationQueue()
    {
        std::vector<std::function<void()>> queueCopy;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            queueCopy.swap(m_creationQueue);
        }
        for (const auto &task : queueCopy)
        {
            task();
        }
    }

    /**
     * @brief
     *
     * @param id
     */
    void DestroyWindow(WindowId id)
    {
        std::lock_guard<std::mutex> lock(m_windowsMutex);
        DestroyWindowInternal(id);
    }

    void HideWindow(WindowId id)
    {
        WindowRuntime *runtime = GetWindowById(id);
        if (runtime && runtime->resource.sdlWindow)
        {
            SDL_HideWindow(runtime->resource.sdlWindow);
        }
    }

    void ShowWindow(WindowId id)
    {
        WindowRuntime *runtime = GetWindowById(id);
        if (runtime && runtime->resource.sdlWindow)
        {
            SDL_ShowWindow(runtime->resource.sdlWindow);
        }
    }

    // Tìm một cửa sổ đã bị ẩn dựa trên template name
    WindowRuntime *FindHiddenWindowByTemplate(const std::string &templateName)
    {
        for (auto &[id, window] : windows)
        {
            if (window->info.templateName == templateName)
            {
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
    WindowRuntime *GetWindowBySDLHandle(SDL_Window *sdlWin)
    {
        if (!sdlWin)
            return nullptr;
        for (auto &[id, runtime] : windows)
        {
            if (runtime->resource.sdlWindow == sdlWin)
            {
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
    std::vector<WindowRuntime *> GetAllWindows()
    {
        std::lock_guard<std::mutex> lock(m_windowsMutex);
        std::vector<WindowRuntime *> result;
        result.reserve(windows.size());
        for (auto &[id, runtime] : windows)
        {
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
    WindowRuntime *GetWindowById(WindowId id)
    {
        std::lock_guard<std::mutex> lock(m_windowsMutex);
        auto it = windows.find(id);
        if (it != windows.end())
        {
            return it->second.get();
        }
        return nullptr;
    }
    /**
     * @brief Get the Main Window object
     *
     * @return WindowRuntime*
     */
    WindowRuntime *GetMainWindow()
    {
        std::lock_guard<std::mutex> lock(m_windowsMutex);
        for (auto &[id, runtime] : windows)
        {
            if (runtime && runtime->style.isMainWindow)
            {
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
    WindowRuntime *GetWindowFromEvent(const SDL_Event *e)
    {
        if (!e)
            return nullptr;

        Uint32 winID = 0;
        switch (e->type)
        {

        // --- Window Events ---
        case SDL_WINDOWEVENT:
            winID = e->window.windowID;
            break;

        // --- Keyboard Events ---
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            winID = e->key.windowID;
            break;
        case SDL_TEXTEDITING:
            winID = e->edit.windowID;
            break;
        case SDL_TEXTINPUT:
            winID = e->text.windowID;
            break;
        case SDL_KEYMAPCHANGED:
            break;

        // --- Mouse Events ---
        case SDL_MOUSEMOTION:
            winID = e->motion.windowID;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            winID = e->button.windowID;
            break;
        case SDL_MOUSEWHEEL:
            winID = e->wheel.windowID;
            break;

        // --- Drag and Drop Events ---
        case SDL_DROPFILE:
        case SDL_DROPTEXT:
        case SDL_DROPBEGIN:
        case SDL_DROPCOMPLETE:
            winID = e->drop.windowID;
            break;

        // --- Display Events ---
        // case SDL_DISPLAYEVENT:          winID = e->display.windowID; break;

        // --- Touch Events ---
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            winID = e->tfinger.windowID;
            break;

        // --- Gesture Events ---
        case SDL_DOLLARGESTURE:
        case SDL_DOLLARRECORD:
        // case SDL_MULTIGESTURE:          winID = e->mgesture.windowID; break;

        // --- Sensor Events ---
        // case SDL_SENSORUPDATE:          winID = e->sensor.windowID; break;

        // Các event còn lại (Joystick, Controller, Audio, User, Quit, v.v.)
        // không thuộc về một window cụ thể nên winID mặc định = 0.
        default:
            break;
        }

        if (winID == 0)
            return nullptr;

        SDL_Window *sdlWin = SDL_GetWindowFromID(winID);
        return GetWindowBySDLHandle(sdlWin);
    }

    /**
     * @brief Lấy con trỏ đến cửa sổ cha của một cửa sổ.
     *
     * @param childId ID của cửa sổ con.
     * @return WindowRuntime* Con trỏ đến cửa sổ cha, hoặc nullptr.
     */
    WindowRuntime *GetParentOf(WindowId childId)
    {
        if (WindowRuntime *child = GetWindowById(childId))
        {
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
    std::vector<WindowRuntime *> GetChildrenOf(WindowId parentId)
    {
        if (WindowRuntime *parent = GetWindowById(parentId))
        {
            return parent->relation.children;
        }
        return {};
    }

    /**
     * @brief Thực thi callback cho từng WindowRuntime một cách an toàn dưới Mutex lock.
     */
    template<typename Func>
    void ForEachWindow(Func&& func) {
        std::vector<WindowRuntime*> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_windowsMutex);
            snapshot.reserve(windows.size());
            for (auto& [id, runtime] : windows) {
                if (runtime) {
                    snapshot.push_back(runtime.get());
                }
            }
        }
        for (auto* runtime : snapshot) {
            func(runtime);
        }
    }

    /**
     * @brief Lấy bản sao danh sách các con trỏ WindowRuntime hiện tại dưới Mutex lock.
     */
    std::vector<WindowRuntime*> GetAllWindows() const {
        std::lock_guard<std::mutex> lock(m_windowsMutex);
        std::vector<WindowRuntime*> list;
        list.reserve(windows.size());
        for (const auto& [id, runtime] : windows) {
            if (runtime) {
                list.push_back(runtime.get());
            }
        }
        return list;
    }

private:
    void DestroyWindowInternal(WindowId id)
    {
        auto it = windows.find(id);
        if (it == windows.end())
            return;

        WindowRuntime *runtime = it->second.get();

        std::vector<WindowRuntime *> childrenCopy = runtime->relation.children;
        for (WindowRuntime *child : childrenCopy)
        {
            DestroyWindowInternal(child->info.id); // Gọi hàm nội bộ
        }

        if (runtime->relation.parent)
        {
            runtime->relation.parent->relation.RemoveChild(runtime);
        }

        windows.erase(it);
    }
};