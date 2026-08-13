// WindowRuntime.h (Cập nhật)
#pragma once
#include <SDL.h>
#include <windows.h>
#include <memory>
#include <mutex> // Thêm vào để sử dụng std::mutex
#include "WindowDefs.h"
#include "WindowPropertyBag.h"
#include "WindowRenderer.h"
#include "WindowController.h"
#include "WindowInfo.h"
#include "WindowResource.h"
#include "WindowRelation.h"
#include "WindowSnapshot.h"
#include "RuntimeFontController.h"


#include "utils.h" 

class FrameTimer;

/**
 * @brief 
 * 
 */
class WindowRuntime {
private:
    std::shared_ptr<const WindowSnapshot> m_currentSnapshot;

public:
    // Mutex để bảo vệ các truy cập đồng thời vào 'state' và các dữ liệu khác
    // từ luồng chính và luồng render.
    mutable std::mutex stateMutex;
    mutable std::mutex frameSyncMutex;

    WindowInfo info;
    WindowState state;
    WindowStyle style;
    PropertyBag properties;
    
    WindowRelation relation;
    WindowResource resource;
 
    std::unique_ptr<WindowRenderer> renderer;
    std::unique_ptr<WindowController> controller;

    std::unique_ptr<FrameTimer> windowloop;

    std::unique_ptr<RuntimeFontController> fontController;


    std::shared_ptr<const WindowSnapshot> CaptureSnapshot() {
        std::lock_guard<std::mutex> lock(stateMutex);

        // Khởi tạo snapshot mới
        auto newSnapshot = std::make_shared<const WindowSnapshot>(
            state
        );

        // Lưu giữ bản snapshot mới nhất
        m_currentSnapshot = newSnapshot;

        return m_currentSnapshot;
    };

    std::shared_ptr<const WindowSnapshot> GetSnapshot() const {
        std::lock_guard<std::mutex> lock(stateMutex);
        return m_currentSnapshot;
    };

    WindowRuntime(WindowId id = 0, SDL_Window* sdlWindow = nullptr, HWND hwnd = nullptr);
    ~WindowRuntime();

    // --- Các hàm tiện ích truy vấn ---
    WindowRuntime* GetParent();
    std::vector<WindowRuntime*> GetChildren();
    bool HasVisibleChildren();
};