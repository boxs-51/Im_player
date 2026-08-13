// MainWindowRenderer.cpp
#include "MainWindowRenderer.h"
#include "settings_manager.h"
#include "globals.h"
#include <popup/popup.h>
#include "player/session/PlayerSession.h"
#include "player/PlayerStateSystem.h"
#include "WindowUtils.h"
#include "WindowRuntime.h"
#include "WindowSnapshot.h"
#include "utils.h"

#include <mpv/render_gl.h>

//#include <mpv_ui.h>
#include "ui/ui.h"


void MainWindowRenderer::Initialize(WindowRuntime* runtime) {
    IMGUI_CHECKVERSION();
    ImGui::SetCurrentContext(runtime->resource.imguiCtx);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    // TẮT tính năng multi-viewport của ImGui để tránh xung đột với WindowManager.
    // Hệ thống của chúng ta đã tự quản lý các cửa sổ OS riêng.
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Logic tải font đã được chuyển ra hàm main()
    CSImGui::InitThemeLibrary(ConfigManager::Instance().GetCommonSettings().themetype);
    ImGui::StyleColorsDark();
    lastInteractionTime = SDL_GetTicks64();
}


void MainWindowRenderer::UpdateUIState(WindowRuntime* runtime, const WindowLayout& layout) {
    ImGuiIO& io = ImGui::GetIO();
    Uint64 currentTime = SDL_GetTicks64();
    
    bool isMouseInsideVideo = (io.MousePos.x >= layout.ClientArea.x && 
                               io.MousePos.x <= (layout.ClientArea.x + layout.ClientArea.w) &&
                               io.MousePos.y >= layout.ClientArea.y && 
                               io.MousePos.y <= (layout.ClientArea.y + layout.ClientArea.h));
    
    bool isInteractingWithUI = io.WantCaptureMouse && (ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered());
    bool isMouseMoving = ((io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) && io.WantCaptureMouse);
    bool isACtionMouse = io.MouseDown[0];

    Uint32 currentTimeout = 300; 
    if (isInteractingWithUI)    currentTimeout = 5000; 
    else if (isMouseInsideVideo) currentTimeout = 1500; 
    bool show_ui_video = runtime->properties.GetValue<bool>("ShowUiVideo", true);
    if ((isMouseMoving || isACtionMouse) && isMouseInsideVideo) {
        lastInteractionTime = currentTime;
        if (!show_ui_video) {
            show_ui_video = true;
            SDL_Event ev;
            ev.type = SDL_CURSOR_EVENT;
            ev.user.code = SDL_ENABLE;
            SDLUtils::SDLX_PushUniqueEvent(ev);
            //SDL_ShowCursor(SDL_ENABLE);
        }
    }

    if (IsAnyPopupOpen()) {
        if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) {
            SDL_Event ev;
            ev.type = SDL_CURSOR_EVENT;
            ev.user.code = SDL_DISABLE;
            SDLUtils::SDLX_PushUniqueEvent(ev);
            //SDL_ShowCursor(SDL_ENABLE);
        }
    }

    if (show_ui_video && (currentTime - lastInteractionTime > currentTimeout)) {
        if (!ImGui::IsAnyItemActive()) {
            show_ui_video = false;
            if (!IsAnyPopupOpen()) {
                SDL_Event ev;
                ev.type = SDL_CURSOR_EVENT;
                ev.user.code = SDL_DISABLE;
                SDLUtils::SDLX_PushUniqueEvent(ev);
                //SDL_ShowCursor(SDL_DISABLE);
            }
        }
    }
    
    runtime->properties.Set<bool>("ShowUiVideo", show_ui_video);
}

void MainWindowRenderer::ShowSubWindows() {
    if (uiState.show_demo)    ImGui::ShowDemoWindow(&uiState.show_demo);
    if (uiState.show_style)   ImGui::ShowStyleEditor();
    if (uiState.show_metrics) ImGui::ShowMetricsWindow();
    if (uiState.show_log)     ImGui::ShowDebugLogWindow();

    if (uiState.show_settings) {
        if (ImGui::Begin("Settings", &uiState.show_settings)) {
            ImGui::Text("🎛️ Bảng điều khiển cửa sổ:");
            ImGui::Separator();
            ImGui::Checkbox("Demo Window", &uiState.show_demo);
            ImGui::Checkbox("Style Editor", &uiState.show_style);
            ImGui::Checkbox("Metrics Window", &uiState.show_metrics);
            ImGui::Checkbox("Debug Log", &uiState.show_log);
        }
        ImGui::End();
    }
}

void MainWindowRenderer::RenderUI(WindowRuntime* runtime, const WindowSnapshot& snapshot) {

    auto* player_state = runtime->resource.GetPlayerSession()->GetState();
    if(!player_state) return;

    UIState uiState;
    uiState.UpdateGlobal();

    const auto& window_state = snapshot.GetState();

    bool isFullscreen = window_state.display.isFullscreen;;
    bool isMaximized = window_state.display.isMaximized;

    const WindowLayout &layout = window_state.geometry.layout;
    
    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        if (uiState.isAnyAction) runtime->state.runtime.is_dirty = true;
    }
    
    
    PlaybackState state = player_state->GetPlaybackState();


    ImGui::SetNextWindowPos(ImVec2(layout.WinX, layout.WinY));
    ImGui::SetNextWindowSize(ImVec2(layout.WinW, layout.WinH));
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    
    ImGui::Begin("WindowMain", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove
    );

    UpdateUIState(runtime, layout);
    
    // Gọi hàm render thanh tiêu đề không viền tùy biến
    std::string title = player_state->GetMediaModel().mediaTitle;

    std::string window_title = TextUtils::TruncateTextByPixels(title.c_str(),(layout.WinW)*0.8f);

    RenderTitleBarWindowObject(runtime, "Media Video Control", window_title.c_str(), layout.TitlePos, layout.TitleSize);

    ImGui::SetNextWindowPos(layout.ClientPos);
    ImGui::BeginChild("##VideoRegion", layout.ClientSize);
    
    ShowSubWindows();

    if (state == PlaybackState::Idle) {
        RenderIdleBackground(AutoPath<std::string>("%ROOT%", "config", "icons", "idle.jpg"), layout.ClientPos, layout.ClientSize);
    }

    bool flagRenderVideo = runtime->properties.GetValue<bool>("RenderVideoFlag", true);
    if (flagRenderVideo || state == PlaybackState::Paused || state == PlaybackState::Seeking ||
        state == PlaybackState::Playing || state == PlaybackState::EndOfFile) {
        if (runtime->resource.GetPlayerSession() && runtime->resource.GetPlayerSession()->GetRenderer() && runtime->resource.graphicsBackend.get())
            runtime->resource.GetPlayerSession()->GetRenderer()->Render(layout.ClientSize, runtime->resource.graphicsBackend.get());
        RenderGhostStatusOverlay(layout.ClientPos, layout.ClientSize, (state == PlaybackState::Paused));
        runtime->properties.Set<bool>("RenderVideoFlag", false);
    }

    if (state == PlaybackState::Playing || state == PlaybackState::Paused || 
        state == PlaybackState::Seeking || state == PlaybackState::EndOfFile) {
        
        if (runtime->resource.GetPlayerSession())
            RenderPlayerControls(runtime, layout.ClientPos, layout.ClientSize);

        RenderSeekingOverlay(runtime, layout.ClientPos, layout.ClientSize);
    }
    
    if (state == PlaybackState::Loading) { 
        RenderLoading(layout.ClientPos, layout.ClientSize);
    }
    
    ImGui::EndChild();
    ImGui::End();

    if (IsAnyPopupOpen()) {
        if (isFullscreen || isMaximized) {
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
        }
        RenderAllPopups(runtime);
    }
}

void MainWindowRenderer::Shutdown() {
    // Không giải phóng thủ công ImGui_Impl ở đây nữa, 
    // vì `runtime->graphicsBackend->Shutdown()` sẽ lo toàn bộ quy trình này một cách an toàn.
}