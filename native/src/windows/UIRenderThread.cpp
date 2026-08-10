#include "UIRenderThread.h"
#include "backends/IGraphicsBackend.h"
#include "WindowManager.h" // Thêm để lấy mutex
#include "WindowRenderer.h"

#include <imgui.h>
#include "common/Exception.h"

UIRenderThread::UIRenderThread(WindowRuntime* owner, IGraphicsBackend* graphicsBackend) 
    : m_ownerRuntime(owner), m_graphicsBackend(graphicsBackend)
{
    if (!m_ownerRuntime || !m_graphicsBackend) {
        THROW_APP_EXCEPTION("UIRenderThread requires a valid WindowRuntime and an initialized graphics backend.");
    }

    // Tạo sub-context đồ họa để luồng này có thể vẽ độc lập
    m_graphicsContext = m_graphicsBackend->CreateSubContext(m_ownerRuntime->resource.sdlWindow);
    if (!m_graphicsContext.has_value()) {
        THROW_APP_EXCEPTION("Failed to create graphics sub-context for UIRenderThread.");
    }
}

UIRenderThread::~UIRenderThread() {
    Stop();
}

void UIRenderThread::Start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&UIRenderThread::Run, this);
    std::string threadName = "UIRenderThread_" + std::to_string(m_ownerRuntime->info.id);
    GetThreadManager().Register(threadName, &m_thread);
}

void UIRenderThread::Stop() {
    if (!m_running) return;
    m_running = false;
    m_cv.notify_one(); // Đánh thức luồng để nó có thể thoát
    if (m_thread.joinable()) {
        m_thread.join();
    }
    std::string threadName = "UIRenderThread_" + std::to_string(m_ownerRuntime->info.id);
    GetThreadManager().Unregister(threadName);
}

void UIRenderThread::RequestRender() {
    {
        std::lock_guard lock(m_mutex);
        m_needsRender = true;
    }
    m_cv.notify_one();
}

void UIRenderThread::RequestResize(int newWidth, int newHeight) {
    {
        std::lock_guard lock(m_mutex);
        m_needsResize = true;
        m_newWidth = newWidth;
        m_newHeight = newHeight;
    }
    m_cv.notify_one();
}

void UIRenderThread::Run() {
try {
    // Kích hoạt context đồ họa cho luồng này
    if (!m_ownerRuntime->resource.graphicsBackend->MakeCurrent(m_ownerRuntime->resource.sdlWindow, m_graphicsContext)) {
        SDL_Log("Error: Could not make graphics context current in UIRenderThread.");
        return;
    }

    // XÓA BỎ LỆNH NÀY. Việc tắt V-Sync (giá trị 0) là một trong những nguyên nhân chính
    // gây ra hiện tượng chớp đen khi resize. Chúng ta sẽ để cho backend
    // (OpenGLBackend) tự quyết định giá trị swap interval, hiện tại đang là 1 (bật V-Sync),
    // giúp đồng bộ hóa việc vẽ và chống xé hình/chớp.
    // SDL_GL_SetSwapInterval(0);
    
    while (m_running) {
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [&] { return m_needsRender || m_needsResize || !m_running; });

            if (!m_running) break;

            // Xử lý thay đổi kích thước TRƯỚC khi render
            if (m_needsResize) {
                m_graphicsBackend->Resize(m_newWidth, m_newHeight);
                m_needsResize = false;
            }
            
            // Nếu không có yêu cầu render, có thể chỉ là yêu cầu resize, quay lại chờ
            //if (!m_needsRender) {
            //    continue;
            //}

            m_needsRender = false; // Reset cờ yêu cầu
        }

        // --- BẮT ĐẦU VÙNG AN TOÀN LUỒNG ---
        // Khóa mutex của runtime để đảm bảo không có luồng nào khác
        // (đặc biệt là luồng chính) thay đổi trạng thái trong khi chúng ta đang vẽ.
        std::lock_guard<std::mutex> stateLock(m_ownerRuntime->stateMutex);

        // Chỉ render nếu cửa sổ đang hiển thị
        if (!m_ownerRuntime->state.display.isVisible) continue;
        
        // Sử dụng FrameTimer của chính cửa sổ này
        if (!m_ownerRuntime->windowloop) continue;
            m_ownerRuntime->windowloop->startFrame();

        // --- LOGIC RENDER ĐƠN GIẢN HÓA ---
        // Luồng này chỉ render cho m_ownerRuntime
        WindowRuntime* currentWindow = m_ownerRuntime;

        // Kích hoạt context đồ họa cho cửa sổ hiện tại
        m_graphicsBackend->MakeCurrent(currentWindow->resource.sdlWindow, m_graphicsContext);

        ImGui::SetCurrentContext(currentWindow->resource.imguiCtx); // Đảm bảo đúng context ImGui
        m_graphicsBackend->BeginFrame(currentWindow->resource.sdlWindow);
        if (currentWindow->renderer) {
            currentWindow->renderer->RenderUI(currentWindow);
        }
        m_graphicsBackend->EndFrame(currentWindow->resource.sdlWindow);
        m_graphicsBackend->SwapWindow(currentWindow->resource.sdlWindow);

        m_ownerRuntime->windowloop->endFrame();

    }
} catch (const std::exception& e) {
    SDL_Log("Exception in UIRenderThread: %s", e.what());
} catch (...) {
    SDL_Log("Unknown exception in UIRenderThread.");
}
}
