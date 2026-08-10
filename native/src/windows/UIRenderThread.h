#pragma once
#include <thread>
#include <atomic>
#include <condition_variable>
#include <memory>

#include "backends/IGraphicsBackend.h"
#include "WindowRuntime.h"
#include "threads/thread_manager.h"

/**
 * @brief Lớp quản lý luồng render giao diện (UI) cho một cửa sổ.
 * Mỗi cửa sổ sẽ có một luồng riêng để vẽ ImGui, tách biệt khỏi luồng chính.
 */
class UIRenderThread {
public:
    UIRenderThread(WindowRuntime* owner, IGraphicsBackend* graphicsBackend);
    ~UIRenderThread();

    UIRenderThread(const UIRenderThread&) = delete;
    UIRenderThread& operator=(const UIRenderThread&) = delete;

    /**
     * @brief Bắt đầu vòng lặp của luồng render.
     */
    void Start();

    /**
     * @brief Dừng luồng render và dọn dẹp.
     */
    void Stop();

    /**
     * @brief Yêu cầu luồng render vẽ một frame mới.
     * Hàm này an toàn để gọi từ các luồng khác.
     */
    void RequestRender();

    /**
     * @brief Yêu cầu luồng render thay đổi kích thước viewport.
     * @param newWidth Chiều rộng mới.
     * @param newHeight Chiều cao mới.
     */
    void RequestResize(int newWidth, int newHeight);

private:
    /**
     * @brief Hàm chính của luồng, chứa vòng lặp render.
     */
    void Run();
    void ProcessEvents();
    
    WindowRuntime* m_ownerRuntime; // Con trỏ không sở hữu đến runtime của cửa sổ
    std::thread m_thread;
    std::string m_registeredThreadName; // Lưu tên đã đăng ký để Unregister an toàn
    std::atomic<bool> m_running = false;

    // Cơ chế đồng bộ
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_needsRender = false;
    bool m_needsResize = false;
    int m_newWidth = 0;
    int m_newHeight = 0;

    // Luồng render của cửa sổ chính sẽ sở hữu backend đồ họa
    IGraphicsBackend* m_graphicsBackend; // Con trỏ không sở hữu, WindowRuntime::resource sở hữu
    // Context đồ họa dành riêng cho luồng này
    std::any m_graphicsContext;
};
