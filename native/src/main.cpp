#define UNICODE
#define _UNICODE
#define SDL_MAIN_HANDLED

#define GL_GLEXT_PROTOTYPES
#include <mpv/mpv_ui.h>
#include <mpv/mpv_controller.h>
#include <mpv/mpv_render_video.h>
#include <mpv/mpv_ui_settings.h>
#include <mpv/mpv_settings.h>
#include <mpv/render_gl.h>

#include "utils.h"
#include "hotkey_handler.h"
#include "globals.h"

#include "services.h"
#include "thread.h"

#include "main.h"

#include "windows/windows_custom_titlebar.h"
#include "windows/windows_borderless.h"
#include "windows/windows_borderless_state.h"

#include "imgui_impl_opengl3.h"
#include "FontManager.h"
//#undef RATE_LIMITED_COUT
//#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <windowsx.h>
#include <csignal>
#include <string>

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
static int frameCount = 0;
static int currentFPS = 0;

static Uint32 flags;

mpv_handle* mpv = nullptr;
mpv_render_context* render_ctx = nullptr;
std::atomic<bool> running(true);


int main(int argc, char** argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InitConsoleSystem();
    //OpenConsoleWindow();
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

    if (!InitMPV(mpv)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể khởi tạo mpv", nullptr);
        Cleanup();
        return 1;
    }

    if (!InitMPVRenderContext(mpv)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể tạo render context từ mpv", nullptr);
        Cleanup();
        return 1;
    }
    
    InitPlaybackStatus(mpv);
    UpdateGlobalWindowLayout(ctx.mainWindow, BW);
    //WaitForServicesReady();
    if (argc >= 2) {
        std::string Url = argv[1];
        CallThread_URLFetch(Url,true);
    }
    Uint32 fpsTimer = SDL_GetTicks();
    while (running) {

        frameStart = SDL_GetTicks();

        flags = SDL_GetWindowFlags(ctx.mainWindow);

        g_WindowVisible = (flags & SDL_WINDOW_SHOWN) && !(flags & SDL_WINDOW_MINIMIZED);
        
        PlaybackState state = GetPlaybackState();


        SDL_Event e;

        while (SDL_PollEvent(&e)) {
            Uint32 eventWindowID = 0;
            Uint32 mainWindowID = 0;
            if (e.type == SDL_WINDOWEVENT || e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEWHEEL || e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                eventWindowID = e.window.windowID;  // Hoặc lấy windowID tương ứng
                mainWindowID = SDL_GetWindowID(ctx.mainWindow);
            }

            if (eventWindowID == mainWindowID) {
                // Xử lý sự kiện cho mainWindow
                HandleMainWindowEvent(e);
            } 
            else {
                if(g_WindowVisible){
                    ImGui_ImplSDL2_ProcessEvent(&e);
                    bool hotkeyHandled = HandleHotkeys(e, mpv, BW.isFullscreen_video, ctx.mainWindow);
                    if (hotkeyHandled) continue;
                }
                
            }
            
        }
        
        if (!g_WindowVisible) {
            if(tool_video){
                mpv_disable_video(mpv);
                tool_video = false;
            }
            SDL_WaitEventTimeout(nullptr, hiddenDelay);

            // Tăng độ trễ dần (giới hạn)
            if (hiddenDelay <= maxHiddenDelay) {
                hiddenDelay += 1000;
            }
            render_load = false;

            frameTime = SDL_GetTicks() - frameStart;
            if (frameDelay > frameTime) {
                SDL_Delay(frameDelay - frameTime);
            }

        } else {
            if (audio_Theme){
                if(tool_video){
                    mpv_disable_video(mpv);
                    tool_video = false;
                }
            } else {
                if(!tool_video){
                    mpv_enable_video(mpv);
                    tool_video = true;
                }
            }
            delayProcessEvents = 0;
            hiddenDelay = 100;  
            ProcessMPVEvents(mpv);
        }

        mpv_update_seek_pending( mpv  );

            UpdateGlobalWindowLayout(ctx.mainWindow, BW);

            //ImGui::SetCurrentContext(ctx.mainImGuiCtx);

            ImFont* cur = FontManager::Instance().GetCurrentFont();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            if (cur) {
                ImGui::PushFont(cur);
                Render(state);
                if(g_DragResizeState.IsFullscreen_video || g_DragResizeState.IsMax || !g_WindowVisible)ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID); 
                if (( IsAnyPopupOpen()) ) {
                    RenderAllPopups(mpv);
                    hasRenderedSomething = true;
                }else{
                    Disabehotkey = false;
                }
                ImGui::PopFont();
            }else{

                Render(state);
                if( g_DragResizeState.IsFullscreen_video || g_DragResizeState.IsMax || !g_WindowVisible)ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID); 
                if (( IsAnyPopupOpen()) ) {
                    RenderAllPopups(mpv);
                    hasRenderedSomething = true;
                } else {
                    Disabehotkey = false;
                }
            }
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
                SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();

                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();

                SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
            }
                    
            if (hasRenderedSomething ) {
                SDL_GL_SwapWindow(ctx.mainWindow);
                frameCount++;
                Uint32 now = SDL_GetTicks();
                if (now - fpsTimer >= 1000) {  // mỗi giây cập nhật
                    g_videoInfo.currentFPS = frameCount;

                if (!fpsInited) {
                    g_videoInfo.minFPS = g_videoInfo.currentFPS;
                    g_videoInfo.maxFPS = g_videoInfo.currentFPS;
                    fpsInited = true;
                } else {
                    if (g_videoInfo.currentFPS < g_videoInfo.minFPS) g_videoInfo.minFPS = g_videoInfo.currentFPS;
                    if (g_videoInfo.currentFPS > g_videoInfo.maxFPS) g_videoInfo.maxFPS = g_videoInfo.currentFPS;
                }

                    frameCount = 0;
                    fpsTimer = now;

                    // (Tùy chọn) log ra console:
                    // std::cout << "FPS: " << currentFPS << std::endl;
                }

                render_video = false;
                render_load = false;
            }
        frameTime = SDL_GetTicks() - frameStart;
        if (frameDelay > frameTime) {
            SDL_Delay(frameDelay - frameTime);
        }
    }

    Cleanup();
    return 0;
}
bool InitMainWindow() {
    ctx.mainWindow = SDL_CreateWindow("Media Video Control",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        BW.WinWidth, BW.WinHeight, SDL_WINDOW_OPENGL |
                                SDL_WINDOW_RESIZABLE |
                               SDL_WINDOW_BORDERLESS | 
                               SDL_WINDOW_SHOWN );

    if (!ctx.mainWindow) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    SetWindowSDL(ctx.mainWindow,640,360,0, 0,0,0,true ,32 );

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

    ImGui_ImplSDL2_InitForOpenGL(ctx.mainWindow, ctx.mainGLContext);
    ImGui_ImplOpenGL3_Init("#version 430 core");
    //FontManager::Instance().LoadFontsSmartMultiAtlas(c_Settings.fontsize, 500 , 200 , 4080, c_Settings.defaultFamily,c_Settings.defaultStyle);
    FontManager::Instance().LoadFontsSmartAuto(c_Settings.fontsize,500,c_Settings.defaultFamily,c_Settings.defaultStyle);
    ImGui::StyleColorsDark();
    return true;
};
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
    if (!g_WindowVisible) {
        // Khi cửa sổ ẩn → chỉ quan tâm đến sự kiện QUIT và USEREVENT
        if (e.type == SDL_QUIT) {
            running = false;
        }
        else if (e.type == SDL_WINDOWEVENT) {

            if (e.window.event == SDL_WINDOWEVENT_SHOWN ||
                e.window.event == SDL_WINDOWEVENT_RESTORED ||
                e.window.event == SDL_WINDOWEVENT_EXPOSED ||
                e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
            {
                // Cửa sổ hiển thị lại → cần render lại frame và UI
            }
        }

        if (e.type == SDL_MPV_RENDER_UPDATE) {
            render_video = true;  
        }
        if (e.type == SDL_MPV_EVENT){
            ProcessMPVEvents(mpv);
        }

    }else{
        ImGui_ImplSDL2_ProcessEvent(&e);
        // 1. Xử lý phím nóng → VD: Ctrl+U mở popup, Space để pause
        bool hotkeyHandled = HandleHotkeys(e , mpv, BW.isFullscreen_video, ctx.mainWindow);
        if (hotkeyHandled){
            return;
        }
        // 2. Phân loại tương tác người dùng

        bool isMousePressOrRelease =
            (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP);
        bool isMouseInteraction =
            e.type == SDL_MOUSEMOTION ||
            e.type == SDL_MOUSEWHEEL;

        bool isWindowInteraction =
            e.type == SDL_WINDOWEVENT &&
            (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
            e.window.event == SDL_WINDOWEVENT_RESIZED ||
            e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            e.window.event == SDL_WINDOWEVENT_EXPOSED ||
            e.window.event == SDL_WINDOWEVENT_MAXIMIZED ||
            e.window.event == SDL_WINDOWEVENT_RESTORED||
            e.window.event == SDL_WINDOWEVENT_SHOWN ||
            e.window.event == SDL_WINDOWEVENT_MOVED  );

        if(isMouseInteraction || isMousePressOrRelease ) NotifyActivity();
        
        if (e.type == SDL_MPV_RENDER_UPDATE) {
            render_video = true;  
        }
        if (e.type == SDL_MPV_EVENT){
            ProcessMPVEvents(mpv);
        }

        if (e.type == SDL_QUIT){
            running = false;
        }
    }
}
void Render( PlaybackState state ){

    glViewport(0, 0, (int)WinW, (int)WinH);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);


    ImGui::SetNextWindowPos(ImVec2(WinX , WinY ));
    ImGui::SetNextWindowSize(ImVec2(WinW, WinH));
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

    UpdateUIState();
    
    RenderBorderlessWindow(ctx.mainWindow,"Media Video Control",BW);

    ShowAllWindows();
    

    if(state == PlaybackState::Idle){
        //GLuint tex_ilde = GetIcon("icon_idle");
        //RenderIdleBackground((ImTextureID)(intptr_t)tex_ilde);
        hasRenderedSomething = true;
    }

    if (render_video || 
        state == PlaybackState::Paused ||  
        state == PlaybackState::EndOfFile ||
        state == PlaybackState::Seeking ||
        state == PlaybackState::Playing) {
        RenderMPVVideo(sdl_rec_to_imvec2_size(Windowlayout.videoArea));
        render_video = false;
        hasRenderedSomething = true;
    }

    if (state == PlaybackState::Playing || 
        state == PlaybackState::Paused  || 
        state == PlaybackState::EndOfFile ){
        RenderPlayerControls(mpv, 
            sdl_rec_to_imvec2_pos(Windowlayout.videoArea), 
            sdl_rec_to_imvec2_size(Windowlayout.videoArea),
            ctx.mainWindow, 
            BW.isFullscreen_video ,
            show_ui_video) ;


        RenderSeekingOverlay(sdl_rec_to_imvec2_pos(Windowlayout.videoArea), 
                            sdl_rec_to_imvec2_size(Windowlayout.videoArea),
                            Seekingdata);
    }
    if (state == PlaybackState::Loading ) { 
        RenderLoading(sdl_rec_to_imvec2_pos(Windowlayout.videoArea), 
                    sdl_rec_to_imvec2_size(Windowlayout.videoArea));

    }

    ImGui::End();

}

