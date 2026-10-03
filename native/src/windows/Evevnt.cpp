#include "EventQueue.h"

#include "WindowRuntime.h"
#include "player/session/PlayerSession.h"
#include "MainWindowRenderer.h"
#include "hotkey_handler.h"
#include <mutex>
#include <memory>
/**
 * @brief
 *
 * @param runtime
 * @param e
 * @param running
 */
void HandleWindowRuntimeEvent(WindowRuntime *runtime, const SDL_Event *e)
{
    if (!runtime)
        return;

    // ImGui SDL event processing mutates the same per-window ImGuiContext and
    // backend data used by UIRenderThread. Serialize the complete event-side
    // mutation against the render frame critical region.
    {
        std::lock_guard<std::mutex> imguiLock(runtime->imguiMutex);
        ImGuiContext *imguiCtx = runtime->resource.imguiCtx;
        if (imguiCtx)
        {
            ImGui::SetCurrentContext(imguiCtx);
        }

        // Backend event handling is part of the same ImGui ownership boundary.
        if (runtime->resource.graphicsBackend)
            runtime->resource.graphicsBackend->ProcessEvent(e);
    }

    auto *session = runtime->resource.GetPlayerSession();
    if (e->type == SDL_MPV_RENDER_UPDATE)
    {
        if (session)
            runtime->properties.Set<bool>("RenderVideoFlag", true);
    }
    if (e->type == SDL_MPV_EVENT)
    {
        if (session && session->GetObserver())
            session->GetObserver()->ProcessEvents();
    }
    if (e->type == SDL_CURSOR_EVENT)
    {
        SDL_ShowCursor(e->user.code);
    }

    if (e->type == SDL_WINDOWEVENT)
    {
        switch (e->window.event)
        {
        case SDL_WINDOWEVENT_RESIZED:
        case SDL_WINDOWEVENT_MOVED:
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
        case SDL_WINDOWEVENT_MINIMIZED:
        case SDL_WINDOWEVENT_HIDDEN:
        case SDL_WINDOWEVENT_SHOWN:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        {
            // Sử dụng hàm định tuyến tự động thay vì gọi cứng hàm cũ
            break;
        }
        case SDL_WINDOWEVENT_CLOSE:
        {
            // Đánh dấu cửa sổ này cần được đóng, thay vì xử lý ngay
            {
                std::lock_guard<std::mutex> lock(runtime->stateMutex);
                runtime->state.runtime.isClosedPending = true;
            }

            break;
        }
        }
    }

    if (HandleHotkeys(e, runtime))
        return;
}