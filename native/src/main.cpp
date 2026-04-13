#define GL_GLEXT_PROTOTYPES

#include <mpv/mpv_ui.h>
#include <mpv/mpv_controller.h>
#include <mpv/mpv_render_video.h>
#include <mpv/mpv_ui_settings.h>
#include <mpv/mpv_settings.h>
#include <mpv/mpv_custom_ui.h>
#include <mpv/render_gl.h>

#include <popup/popup.h>

#include "utils.h"
#include "hotkey_handler.h"
#include "globals.h"

#include <services/services_services.h>
#include <threads/thread.h>

#include "main.h"
#include "notification.h"

#include <windows/windows_borderless.h>
#include <windows/windows_borderless_state.h>

#include "imgui_impl_opengl3.h"
#include "FontManager.h"
#undef RATE_LIMITED_COUT
#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <windowsx.h>
#include <csignal>
#include <string>


static bool render_video = false;
static bool show_ui_video = false;
static bool g_WindowVisible = false;

static Uint32 flags;
std::atomic<bool> running(true);

// NVIDIA GPU
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}

// AMD GPU
extern "C" {
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

void ShowAllWindows()
{
    // ======== Các cửa sổ phụ ========
    if (uiState.show_demo)
        ImGui::ShowDemoWindow(&uiState.show_demo);

    if (uiState.show_style)
        ImGui::ShowStyleEditor();

    if (uiState.show_metrics)
        ImGui::ShowMetricsWindow();

    if (uiState.show_log)
        ImGui::ShowDebugLogWindow();

    // ======== Cửa sổ Settings chính ========
    if (uiState.show_settings)
    {
        // Dùng &uiState.show_settings để tự tắt khi ấn X
        if (ImGui::Begin("Settings", &uiState.show_settings))
        {
            ImGui::Text("🎛️ Bảng điều khiển cửa sổ:");
            ImGui::Separator();

            // Checkbox bật/tắt từng cửa sổ khác
            ImGui::Checkbox("Demo Window", &uiState.show_demo);
            ImGui::Checkbox("Style Editor", &uiState.show_style);
            ImGui::Checkbox("Metrics Window", &uiState.show_metrics);
            ImGui::Checkbox("Debug Log", &uiState.show_log);
        }
        ImGui::End();
    }
}
void ShutdownMainWindow() {
    if (ctx.mainImGuiCtx) {
        ImGui::SetCurrentContext(ctx.mainImGuiCtx);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext(ctx.mainImGuiCtx);
        ctx.mainImGuiCtx = nullptr;
    }
    if (ctx.mainGLContext) {
        SDL_GL_DeleteContext(ctx.mainGLContext);
        ctx.mainGLContext = nullptr;
    }
    if (ctx.mainWindow) {
        SDL_DestroyWindow(ctx.mainWindow);
        ctx.mainWindow = nullptr;
    }
}
void Cleanup() {

    StopService();
    ShutdownMainWindow();

    CleanupIcons();
    CleanupMPV();

    SDL_Quit();
}
void HandleMainWindowEvent(const SDL_Event& e) {

    if (e.type == SDL_MPV_RENDER_UPDATE_SYNC) render_video = true;  
    if (e.type == SDL_MPV_EVENT)ProcessMPVEvents(mpv.mpv);
    if (e.type == SDL_QUIT)running = false;
    if (HandleHotkeys(e , mpv.mpv))return;
    if(g_WindowVisible) {
        bool isMousePressOrRelease =
            (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP);
        bool isMouseInteraction =
            e.type == SDL_MOUSEMOTION ||
            e.type == SDL_MOUSEWHEEL;
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        SDL_Point mousePos = { mouseX, mouseY };
        bool isMouseInsideVideo = SDL_PointInRect(&mousePos, &Windowlayout.videoArea);
        if (isMousePressOrRelease && isMouseInsideVideo) {
            NotifyActivity(show_ui_video);
        }
    }
    if (e.type == SDL_WINDOWEVENT) {
        if (e.window.event == SDL_WINDOWEVENT_RESIZED || 
            e.window.event == SDL_WINDOWEVENT_MOVED ||
            e.window.event == SDL_WINDOWEVENT_MAXIMIZED ||
            e.window.event == SDL_WINDOWEVENT_RESTORED) {
            
            // Chỉ cập nhật khi có sự kiện thay đổi thực sự
            UpdateGlobalWindowLayout(ctx.mainWindow, g_DragResizeState , Windowlayout);
        }
    }
    
}
bool InitMainWindow() {

    Uint32 windowFlags = SDL_WINDOW_OPENGL|
                        SDL_WINDOW_RESIZABLE|
                        SDL_WINDOW_SHOWN|
                        SDL_WINDOW_ALLOW_HIGHDPI;
    #ifdef CUSTOM_TITLEBAR
    windowFlags |= SDL_WINDOW_BORDERLESS;
    #endif
    ctx.mainWindow = SDL_CreateWindow("Media Video Control",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, windowFlags);

    if (!ctx.mainWindow) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    #ifdef CUSTOM_TITLEBAR
    SDLUtils::SetWindowSDL(ctx.mainWindow,720,360);
    #else 
    SDL_SetWindowMinimumSize(ctx.mainWindow,640, 360);
    #endif
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    ctx.mainGLContext = SDL_GL_CreateContext(ctx.mainWindow);
    if (!ctx.mainGLContext) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(ctx.mainWindow);
        return false;
    }

    // Make current before gl loader init
    if (SDL_GL_MakeCurrent(ctx.mainWindow, ctx.mainGLContext) != 0) {
        SDL_Log("SDL_GL_MakeCurrent failed: %s", SDL_GetError());
        SDL_GL_DeleteContext(ctx.mainGLContext);
        SDL_DestroyWindow(ctx.mainWindow);
        return false;
    }

    if (gl3wInit() != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenGL Error", "Can't init gl3w!", nullptr);
        SDL_GL_DeleteContext(ctx.mainGLContext);
        SDL_DestroyWindow(ctx.mainWindow);
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    // ImGui main context
    IMGUI_CHECKVERSION();
    ctx.mainImGuiCtx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx.mainImGuiCtx);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    } 
    LoadSettings();

    ApplyTheme(c_Settings.themetype);

    ImGui_ImplSDL2_InitForOpenGL(ctx.mainWindow, ctx.mainGLContext);
    ImGui_ImplOpenGL3_Init("#version 430 core");

    FontManager::Instance().LoadFontsSpecific(c_Settings.fontsize, AutoPath<std::string>("%ROOT%","fonts"));

    ImGui::StyleColorsDark();

    return true;
};
void RenderUI( PlaybackState state ){
    glViewport(0, 0, (int)Windowlayout.WinW, (int)Windowlayout.WinH);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::SetNextWindowPos(ImVec2(Windowlayout.WinX , Windowlayout.WinY ));
    ImGui::SetNextWindowSize(ImVec2(Windowlayout.WinW, Windowlayout.WinH));
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID); 
    ImGui::Begin("VideoRegion", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoScrollbar  |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus   |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoMove
    );

    UpdateUIState( show_ui_video );
    
    RenderBorderlessWindow(ctx.mainWindow,"Media Video Control",g_DragResizeState ,Windowlayout.WinDowPos, Windowlayout.WinDowSize);

    
    ShowAllWindows();

    
    if(state == PlaybackState::Idle){
        RenderIdleBackground(AutoPath<std::string>("%ROOT%" , "icons","idle.jpg") ,Windowlayout.VideoPos, Windowlayout.VideoSize);
    }

    if (render_video ||
        state == PlaybackState::Paused  || 
        state == PlaybackState::Seeking  ||
        state == PlaybackState::Playing) {
        RenderMPVVideo(Windowlayout.VideoSize);
        DrawGhostStatusOverlay(Windowlayout.VideoPos, Windowlayout.VideoSize, state == PlaybackState::Paused);
        render_video = false;
    }

    if (state == PlaybackState::Playing || 
        state == PlaybackState::Paused  || 
        state == PlaybackState::Seeking ||
        state == PlaybackState::EndOfFile ){
        RenderPlayerControls(mpv.mpv, 
            Windowlayout.VideoPos, 
            Windowlayout.VideoSize,
            ctx.mainWindow, 
            g_DragResizeState.IsFullscreen_video,
            show_ui_video);


        RenderSeekingOverlay(Windowlayout.VideoPos, 
                            Windowlayout.VideoSize,
                            dataseek);
    }
    if (state == PlaybackState::Loading ) { 
        RenderLoading(Windowlayout.VideoPos, 
                        Windowlayout.VideoSize);

    }
    
    ImGui::End();

}
void RenderPushFont(PlaybackState state ,bool g_WindowVisible){
    RenderUI(state);
    if(g_DragResizeState.IsFullscreen_video || g_DragResizeState.IsMax || !g_WindowVisible)ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID); 
    if (( IsAnyPopupOpen()) ) RenderAllPopups();
    else Disabehotkey = false;
}
void RenderFrame(bool g_WindowVisible){
    static Uint64 lastVisibleTime = 0;
    static bool isRenderingPaused = false;

    if (!g_WindowVisible) {
        // Nếu vừa mới ẩn, ghi lại thời điểm cuối cùng còn nhìn thấy
        if (lastVisibleTime == 0) {
            lastVisibleTime = SDL_GetTicks64();
        }

        // Kiểm tra xem đã quá 1 giây chưa
        if (SDL_GetTicks64() - lastVisibleTime > 2000) { 
            if (!isRenderingPaused) {
                mpv_disable_video(mpv.mpv);
                isRenderingPaused = true;
                // LOG: "Video rendering paused after timeout"
            }
            return; // Dừng xử lý loop render ở đây để tiết kiệm CPU/GPU
        }
    } else {
        // Khi cửa sổ hiện trở lại: Reset ngay lập tức
        lastVisibleTime = 0;
        if (isRenderingPaused) {
            if (!Audio_visualizers) {
                mpv_enable_video(mpv.mpv);
            }
            isRenderingPaused = false;
        }
    }

    // Logic render chính vẫn chạy nếu chưa quá 1s hoặc đang Visible
    if (Audio_visualizers) {
        mpv_disable_video(mpv.mpv);
    } else if (!isRenderingPaused) {
        mpv_enable_video(mpv.mpv);
    }

    PlaybackState state = GetPlaybackState();

    //ImFont* cur = FontManager::Instance().GetCurrentFont();

    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    //if (cur) {
    //    ImGui::PushFont(cur);
    //    RenderPushFont(state,g_WindowVisible);
    //    ImGui::PopFont();
    //}else{
        RenderPushFont(state,g_WindowVisible);
    //}
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();

        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();

        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        
    }
            
    
    SDL_GL_SwapWindow(ctx.mainWindow);

}
int main(int argc, char** argv) {

    //InitNotification();

    SDL_SetMainReady();

    InitConsoleSystem();
    InitThemeLibrary();

    StartRuntimeServices(); 
    std::set_terminate(TerminateHandler);

    std::signal(SIGSEGV, SignalHandler);
    std::signal(SIGABRT, SignalHandler);
    std::signal(SIGFPE, SignalHandler);
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    

    if (!InitMainWindow()){
        Cleanup();
        return 1;
    }

    if (!InitMPV(mpv.mpv)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể khởi tạo mpv", nullptr);
        Cleanup();
        return 1;
    }

    if (!InitMPVRenderContext(mpv.mpv)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể tạo render context từ mpv", nullptr);
        Cleanup();
        return 1;
    }
    UpdateGlobalWindowLayout(ctx.mainWindow, g_DragResizeState , Windowlayout);
    #ifdef RENDER_MPV_THREAD
    StartMPVRenderThread();

    #endif
    
    InitPlaybackStatus(mpv.mpv);
    
    if (argc >= 2) {
        std::string Url = argv[1];
        CallThread_URLFetch(Url,true);
    }

    FrameTimer fpsLimiter(60);
    SDL_Event e;
    while (running) {
        fpsLimiter.startFrame();
        UpdateTheme(ImGui::GetIO().DeltaTime);
        flags = SDL_GetWindowFlags(ctx.mainWindow);
        if(ToggleFullscreen){
            SDLUtils::SDLX_ToggleFullscreen(ctx.mainWindow, !g_DragResizeState.IsFullscreen_video);
            ToggleFullscreen =false;
        }
        g_WindowVisible = (flags & SDL_WINDOW_SHOWN) && !(flags & SDL_WINDOW_MINIMIZED );
        g_DragResizeState.IsMax = (flags & SDL_WINDOW_MAXIMIZED) != 0;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            HandleMainWindowEvent(e);
        }
    
        mpv_update_seek_pending( mpv.mpv );

        RenderFrame(g_WindowVisible) ;

        g_videoInfo.currentFPS = fpsLimiter.getFPS();
        fpsLimiter.endFrame();
    }

    Cleanup();
    return 0;
}