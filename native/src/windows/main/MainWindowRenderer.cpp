// MainWindowRenderer.cpp
#include "MainWindowRenderer.h"
#include "MainWindowState.h"
#include "FontManager.h"
#include "settings_manager.h"
#include "globals.h"
#include <popup/popup.h>
#include <mpv/session/MPVSession.h>

#include <mpv/mpv_ui.h>
#include <mpv/mpv_ui_settings.h>
#include <mpv/render_gl.h>
#include <mpv/mpv_data.h>

void MainWindowRenderer::Initialize(WindowRuntime* runtime) {
    IMGUI_CHECKVERSION();
    
    // Khởi tạo ImGui Context riêng cho cửa sổ này và lưu vào PropertyBag
    //ImGuiContext* imguiCtx = ImGui::CreateContext();
    ImGui::SetCurrentContext(runtime->imguiCtx);
    //runtime->properties.Set<ImGuiContext*>("ImGuiCtx", imguiCtx);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    CSImGui::InitThemeLibrary(ConfigManager::Instance().GetCommonSettings().themetype);

    FontManager::Instance().LoadFontsSpecific(
        ConfigManager::Instance().GetCommonSettings().fontsize, 
        AutoPath<std::string>("%ROOT%", "config", "fonts")
    );
    ImGui::StyleColorsDark();
    lastInteractionTime = SDL_GetTicks64();
}

static bool show_ui_video = true;

void MainWindowRenderer::UpdateUIState(WindowRuntime* runtime) {
    ImGuiIO& io = ImGui::GetIO();
    Uint64 currentTime = SDL_GetTicks64();
    
    auto layout = runtime->properties.Get<MainWindowLayout>("Layout");

    bool isMouseInsideVideo = (io.MousePos.x >= layout.videoArea.x && 
                               io.MousePos.x <= (layout.videoArea.x + layout.videoArea.w) &&
                               io.MousePos.y >= layout.videoArea.y && 
                               io.MousePos.y <= (layout.videoArea.y + layout.videoArea.h));
    
    bool isInteractingWithUI = io.WantCaptureMouse && (ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered());
    bool isMouseMoving = ((io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) && io.WantCaptureMouse);

    Uint32 currentTimeout = 300; 
    if (isInteractingWithUI)    currentTimeout = 5000; 
    else if (isMouseInsideVideo) currentTimeout = 1500; 
        
    if (isMouseMoving && isMouseInsideVideo) {
        lastInteractionTime = currentTime;
        if (!show_ui_video) {
            show_ui_video = true;
            SDL_ShowCursor(SDL_ENABLE);
        }
    }

    if (IsAnyPopupOpen()) {
        if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) SDL_ShowCursor(SDL_ENABLE);
    }

    if (show_ui_video && (currentTime - lastInteractionTime > currentTimeout)) {
        if (!ImGui::IsAnyItemActive()) {
            show_ui_video = false;
            if (!IsAnyPopupOpen()) SDL_ShowCursor(SDL_DISABLE);
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
void RenderTitleBarWindowObject(WindowRuntime* runtime, const char* title, ImVec2 _winPos, ImVec2 _winSize){
    
    #ifdef CUSTOM_TITLEBAR

    if (!runtime->sdlWindow || runtime->state.isFullscreen) return;

    ImVec2 titlePos = (_winPos);
    ImVec2 titleSize = (_winSize);
    float dt = ImGui::GetIO().DeltaTime;
    ImGui::SetNextWindowPos(titlePos);
    ImGui::BeginChild("##TitleBar",titleSize);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::SetCursorScreenPos(titlePos);
    ImGui::InvisibleButton("TitleBarRegion", titleSize);
    //bool hoveredTitle = ImGui::IsItemHovered();
    //ImU32 btnBgColor = hoveredTitle ? IM_COL32(60,60,60,255) : IM_COL32(35,35,35,255);
    ImU32 btnBgColor = IM_COL32(35,35,35,255);
    dl->AddRectFilled(titlePos, titlePos + titleSize, btnBgColor);

    dl->AddText(
        ImVec2(titlePos.x + 10.0f, titlePos.y + (runtime->style.titleHeight - ImGui::GetTextLineHeight()) * 0.5f),
        IM_COL32(255,255,255,255),
        title
    );

    const ImVec2 btnSize(runtime->style.btnSize , runtime->style.titleHeight ); // giữ nguyên chiều cao
    const float pad = 8.0f;

    // Close ❌
    ImVec2 closePos(titlePos.x + titleSize.x - btnSize.x , titlePos.y );
    ImGui::SetCursorScreenPos(closePos);

    // Kiểm tra trạng thái hover / click
    bool clickedClose = ImGui::InvisibleButton("CloseBtn", btnSize);
    bool hoveredClose = ImGui::IsItemHovered();
    bool activeClose = (ImGui::IsItemHovered() && (runtime->state.mouseDownClose));
    CSImGui::ToolTip("Close", 2.0f, ToolTipFlags_Animation);
    if(clickedClose || hoveredClose || activeClose) is_dirty = true;
    // Điều chỉnh màu dựa vào trạng thái
    static ImVec4 closeColor = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);
    ImVec4 target_close = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);
    if (hoveredClose) target_close = ImVec4(0.90f, 0.23f, 0.23f, 1.0f);
    if (activeClose) target_close = ImVec4(0.75f, 0.10f, 0.10f, 1.0f);
    closeColor = ImLerp(closeColor, target_close, SMOOTH_LERP(12.0f, dt));

    dl->AddRectFilled(closePos, closePos + btnSize, ToCol32(closeColor), 4.0f);
    {
        // ----- Xác định center -----
        float iconPad   = 7.0f;   
        float lineThick = 2.0f;
        float lineScale = 1.0f;
        float linesize1 = 30.0f;
        float linesize2 = 30.0f;

        // Giao điểm trung tâm của X
        ImVec2 center = ImVec2(closePos.x + btnSize.x * 0.5f, closePos.y + btnSize.y * 0.5f);

        // Tùy chỉnh góc từng thanh tại giao điểm
        float angle1 = 45.0f;   // góc đường chéo 1 (độ) so với trục ngang
        float angle2 = -45.0f;  // góc đường chéo 2 (độ) so với trục ngang

        // Tùy chỉnh chiều dài từng thanh
        float halfLen1 = (linesize1 * 0.5f - iconPad) * lineScale;
        float halfLen2 = (linesize2 * 0.5f - iconPad) * lineScale;

        // Hàm chuyển độ sang rad
        auto Deg2Rad = [](float deg){ return deg * 3.14159265f / 180.0f; };

        // Vector đường chéo 1
        ImVec2 dir1 = ImVec2(cosf(Deg2Rad(angle1)) * halfLen1,
                            sinf(Deg2Rad(angle1)) * halfLen1);

        // Vector đường chéo 2
        ImVec2 dir2 = ImVec2(cosf(Deg2Rad(angle2)) * halfLen2,
                            sinf(Deg2Rad(angle2)) * halfLen2);

        // Tính điểm đầu cuối từng đường chéo dựa vào center
        ImVec2 c1 = center - dir1;
        ImVec2 c2 = center + dir1;

        ImVec2 c3 = center - dir2;
        ImVec2 c4 = center + dir2;

        // Vẽ 2 đường chéo
        ImU32 color = IM_COL32(255,255,255,255);
        dl->AddLine(c1, c2, color, lineThick);
        dl->AddLine(c3, c4, color, lineThick);
    }

    // Maximize 🗖 / Restore 🗗
    
    ImVec2 maxPos(closePos.x - btnSize.x , titlePos.y );
    ImGui::SetCursorScreenPos(maxPos);

    bool clickedMax =ImGui::InvisibleButton("MaxRestoreBtn", btnSize);
    bool hoveredMax = ImGui::IsItemHovered();
    bool activeMax  = (ImGui::IsItemHovered() && (runtime->state.mouseDownMax || runtime->state.mouseDownRestore));

    CSImGui::ToolTip(runtime->state.isMaximized ? "Restore" : "Maximize", 2.0f, ToolTipFlags_Animation);

    if(clickedMax || hoveredMax || activeMax) is_dirty = true;

    static ImVec4 maxColor = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);
    ImVec4 targetMax = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);
    if (hoveredMax) targetMax = ImVec4(0.235f, 0.235f,0.235f, 1.0f);
    if (activeMax) targetMax = ImVec4(0.353f , 0.353f, 0.353f, 1.0f);
        
    maxColor = ImLerp(maxColor, targetMax, SMOOTH_LERP(12.0f, dt));
        
    dl->AddRectFilled(maxPos, maxPos + btnSize, ToCol32(maxColor), 4.0f);
    {
        float iconPad     = 7.0f;    // khoảng cách từ viền nút đến icon
        float iconBorder  = 0.0f;    // độ dày viền (stroke)
        float iconRounding = 1.0f;   // bán kính bo góc
        float iconScaleY  = 0.8f;    // tỉ lệ chiều cao
        float iconScaleX  = 0.8f;    // tỉ lệ chiều rộng
        if (!runtime->state.isMaximized) {
            float iconScaleY  = 1.0f;    // tỉ lệ chiều cao
            ImVec2 iconPos = ImVec2(maxPos.x + iconPad, maxPos.y + iconPad);
            ImVec2 iconSize = ImVec2(
                (btnSize.x - 2 * iconPad) * iconScaleX,
                (btnSize.y - 2 * iconPad) * iconScaleY
            );
            dl->AddRect(iconPos, iconPos + iconSize, IM_COL32(255,255,255,255), iconRounding, 0, iconBorder);
        } else {
            ImVec2 backPos  = ImVec2(maxPos.x + iconPad + 2, maxPos.y + iconPad + 2);
            ImVec2 frontPos = ImVec2(maxPos.x + iconPad,     maxPos.y + iconPad);
            ImVec2 iconSize = ImVec2(
                (btnSize.x - 2 * iconPad - 2) * iconScaleX,
                (btnSize.y - 2 * iconPad - 2) * (iconScaleY + 0.4f)
            );
            dl->AddRect(backPos,  backPos  + iconSize, IM_COL32(255,255,255,255), iconRounding, 0, iconBorder);
            dl->AddRect(frontPos, frontPos + iconSize, IM_COL32(255,255,255,255), iconRounding, 0, iconBorder);
        }
    }

    // Minimize ➖
    ImVec2 minPos(maxPos.x - btnSize.x , titlePos.y );
    ImGui::SetCursorScreenPos(minPos);
    
    bool clickedMin = ImGui::InvisibleButton("MinBtn", btnSize);
    bool hoveredMin = ImGui::IsItemHovered();
    bool activeMin  = (ImGui::IsItemHovered() && runtime->state.mouseDownMin);
    if(clickedMin || hoveredMin || activeMin) is_dirty = true;

    CSImGui::ToolTip("Minimize", 2.0f, ToolTipFlags_Animation);

    static ImVec4 minColor = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);
    ImVec4 target_min = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);
    if (hoveredMin) target_min = ImVec4(0.235f, 0.235f, 0.235f, 1.0f);
    if (activeMin)  target_min = ImVec4(0.353f, 0.353f, 0.353f, 1.0f);
    
    minColor = ImLerp(minColor, target_min, SMOOTH_LERP(12.0f, dt));
    dl->AddRectFilled(minPos, minPos + btnSize, ToCol32(minColor), 4.0f);
    {
        float linesize = 30.f;
        ImVec2 lineStart = ImVec2(minPos.x + pad, minPos.y + btnSize.y / 2);
        ImVec2 lineEnd   = ImVec2(minPos.x + linesize - pad, minPos.y + btnSize.y / 2);
        dl->AddLine(lineStart, lineEnd, IM_COL32(255,255,255,255), 2.0f);
    }
    ImGui::EndChild();
    #endif
}
void MainWindowRenderer::RenderUI(WindowRuntime* runtime) {
    auto layout = runtime->properties.Get<MainWindowLayout>("Layout");
    PlaybackState state = GetPlaybackState();

    ImGui::SetNextWindowPos(ImVec2(layout.WinX, layout.WinY));
    ImGui::SetNextWindowSize(ImVec2(layout.WinW, layout.WinH));
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    
    ImGui::Begin("WindowMain", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove
    );

    UpdateUIState(runtime);
    
    // Gọi hàm render thanh tiêu đề không viền tùy biến
    extern void RenderTitleBarWindowObject(WindowRuntime* runtime, const char* title, ImVec2 _winPos, ImVec2 _winSize);
    RenderTitleBarWindowObject(runtime, "Media Video Control", layout.TitlePos, layout.TitleSize);

    ShowSubWindows();

    ImGui::SetNextWindowPos(layout.VideoPos);
    ImGui::BeginChild("##VideoRegion", layout.VideoSize);
    
    if (state == PlaybackState::Idle) {
        RenderIdleBackground(AutoPath<std::string>("%ROOT%", "config", "icons", "idle.jpg"), layout.VideoPos, layout.VideoSize);
    }

    bool flagRenderVideo = runtime->properties.Get<bool>("RenderVideoFlag", false);
    if (flagRenderVideo || state == PlaybackState::Paused || state == PlaybackState::Seeking ||
        state == PlaybackState::Playing || state == PlaybackState::EndOfFile) {
        if (runtime->mpvSession && runtime->mpvSession->GetRenderer())
            runtime->mpvSession->GetRenderer()->Render(layout.VideoSize, runtime->graphicsBackend.get());
        RenderGhostStatusOverlay(layout.VideoPos, layout.VideoSize, (state == PlaybackState::Paused));
        runtime->properties.Set<bool>("RenderVideoFlag", false);
    }

    if (state == PlaybackState::Playing || state == PlaybackState::Paused || 
        state == PlaybackState::Seeking || state == PlaybackState::EndOfFile) {
        
        //RenderPlayerControls(mpv.mpv, layout.VideoPos, layout.VideoSize,
        if (runtime->mpvSession)
            RenderPlayerControls(runtime, layout.VideoPos, layout.VideoSize, runtime->state.isFullscreen, show_ui_video);

        RenderSeekingOverlay(layout.VideoPos, layout.VideoSize);
    }
    
    if (state == PlaybackState::Loading) { 
        RenderLoading(layout.VideoPos, layout.VideoSize);
    }
    
    ImGui::EndChild();
    ImGui::End();

    if (IsAnyPopupOpen()) {
        if (runtime->state.isFullscreen || runtime->state.isMaximized) {
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
        }
        RenderAllPopups();
    }
}

void MainWindowRenderer::Shutdown() {
    // Không giải phóng thủ công ImGui_Impl ở đây nữa, 
    // vì `runtime->graphicsBackend->Shutdown()` sẽ lo toàn bộ quy trình này một cách an toàn.
}