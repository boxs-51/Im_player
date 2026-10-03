#include "UIRenderThread.h"
#include "backends/IGraphicsBackend.h"
#include "WindowManager.h" // Thêm để lấy mutex
#include "WindowRenderer.h"
#include "UpdateWindowState.h"
#include "WindowDefs.h"
#include "WindowSnapshot.h"

#include <imgui.h>
#include "common/Exception.h"
#include "common/LifecycleEvidence.h"

UIRenderThread::UIRenderThread(WindowRuntime *owner, IGraphicsBackend *graphicsBackend)
    : m_ownerRuntime(owner), m_graphicsBackend(graphicsBackend)
{
    if (!m_ownerRuntime || !m_graphicsBackend)
    {
        THROW_APP_EXCEPTION("UIRenderThread requires a valid WindowRuntime and an initialized graphics backend.");
    }

    // Tạo sub-context đồ họa để luồng này có thể vẽ độc lập
    m_graphicsContext = m_graphicsBackend->CreateSubContext(m_ownerRuntime->resource.sdlWindow);
    if (!m_graphicsContext.has_value())
    {
        THROW_APP_EXCEPTION("Failed to create graphics sub-context for UIRenderThread.");
    }

    LifecycleEvidence::Emit(
        "UIRenderThread",
        "CREATE",
        LifecycleEvidence::PointerIdentity(this));
}

UIRenderThread::~UIRenderThread()
{
    Stop();
    LifecycleEvidence::Emit(
        "UIRenderThread",
        "DESTROY",
        LifecycleEvidence::PointerIdentity(this));
}

void UIRenderThread::Start()
{
    if (m_running)
        return;
    m_running = true;

    m_thread = std::thread(&UIRenderThread::Run, this);
    m_registeredThreadName = "UIRenderThread_" + std::to_string(m_ownerRuntime->info.id);
    GetThreadManager().Register(m_registeredThreadName, &m_thread);
    LifecycleEvidence::Emit("UIRenderThread", "START", LifecycleEvidence::PointerIdentity(this));
}

void UIRenderThread::Stop()
{
    if (!m_running)
        return;

    const std::string lifecycleId = LifecycleEvidence::PointerIdentity(this);
    LifecycleEvidence::Emit("UIRenderThread", "STOP", lifecycleId);
    m_running = false;
    m_cv.notify_one(); // Đánh thức luồng để nó có thể thoát
    if (m_thread.joinable())
    {
        m_thread.join();
        LifecycleEvidence::Emit("UIRenderThread", "JOIN", lifecycleId);
    }

    if (!m_registeredThreadName.empty())
    {
        GetThreadManager().Unregister(m_registeredThreadName);
        m_registeredThreadName.clear();
    }
}

void UIRenderThread::RequestRender()
{
    {
        std::lock_guard lock(m_mutex);
        m_needsRender = true;
    }
    m_cv.notify_one();
}

void UIRenderThread::RequestResize(int newWidth, int newHeight)
{
    {
        std::lock_guard lock(m_mutex);
        m_needsResize = true;
        m_newWidth = newWidth;
        m_newHeight = newHeight;
    }
    m_cv.notify_one();
}

void UIRenderThread::Run()
{
    try
    {
        // Kích hoạt context đồ họa cho luồng này
        if (!m_ownerRuntime->resource.graphicsBackend->MakeCurrent(m_ownerRuntime->resource.sdlWindow, m_graphicsContext))
        {
            SDL_Log("Error: Could not make graphics context current in UIRenderThread.");
            return;
        }

        while (m_running)
        {
            {
                std::unique_lock lock(m_mutex);
                m_cv.wait(lock, [&]
                          { return m_needsRender || m_needsResize || !m_running; });

                if (!m_running)
                    break;

                // Xử lý thay đổi kích thước TRƯỚC khi render
                if (m_needsResize)
                {
                    m_graphicsBackend->Resize(m_newWidth, m_newHeight);
                    m_needsResize = false;
                }

                // Nếu không có yêu cầu render, có thể chỉ là yêu cầu resize, quay lại chờ
                if (!m_needsRender) {
                    continue;
                }

                m_needsRender = false; // Reset cờ yêu cầu
            }

            {
                std::lock_guard<std::mutex> lock(m_ownerRuntime->stateMutex);
                // Chỉ render nếu cửa sổ đang hiển thị
                if (!m_ownerRuntime->state.display.isVisible)
                    continue;
            }
            
            // Sử dụng FrameTimer của chính cửa sổ này
            if (!m_ownerRuntime->windowloop)
                continue;

            {
    
                //std::lock_guard<std::mutex> lock(m_ownerRuntime->frameSyncMutex);
                std::shared_ptr<const WindowSnapshot> snapshot = m_ownerRuntime->CaptureSnapshot();
 
                m_ownerRuntime->windowloop->startFrame();

                // --- LOGIC RENDER ĐƠN GIẢN HÓA ---
                // Luồng này chỉ render cho m_ownerRuntime
                WindowRuntime *currentWindow = m_ownerRuntime;
                
                // Kích hoạt context đồ họa cho cửa sổ hiện tại và kiểm tra kết quả
                if (!m_graphicsBackend->MakeCurrent(currentWindow->resource.sdlWindow, m_graphicsContext))
                {
                    SDL_Log("UIRenderThread Error: MakeCurrent failed for window ID %u", currentWindow->info.id);
                    // Giải phóng lock trước khi sleep/yield để tránh giữ stateMutex gây deadlock cho Main Thread
                    m_ownerRuntime->windowloop->endFrame();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                if (currentWindow->resource.imguiCtx)
                {
                    ImGui::SetCurrentContext(currentWindow->resource.imguiCtx);
                } // Đảm bảo đúng context ImGui

                
                m_graphicsBackend->BeginFrame(currentWindow->resource.sdlWindow);
                
                if (currentWindow->renderer)
                {
                    currentWindow->renderer->RenderUI(currentWindow, *snapshot);
                }

                m_graphicsBackend->EndFrame(currentWindow->resource.sdlWindow);
                m_graphicsBackend->SwapWindow(currentWindow->resource.sdlWindow);

                m_ownerRuntime->windowloop->endFrame();
            }
        }
    }
    catch (const std::exception &e)
    {
        SDL_Log("Exception in UIRenderThread: %s", e.what());
    }
    catch (...)
    {
        SDL_Log("Unknown exception in UIRenderThread.");
    }
}
