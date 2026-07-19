// main.cpp
#include <SDL.h>
#include <windows.h>
#include <csignal>
#include "utils.h"

#include <gui/gui.h>
#include <mpv/mpv_ui_settings.h>
#include <MPVManager.h>
#include <mpv/mpv_data.h>
#include <mpv/audio/filter/af_m.h>
#include "settings_manager.h"
#include <popup/popup.h>
#include "hotkey_handler.h"
#include <backends/backend.h>
#include <threads/thread_manager.h>
#include <log.h>
#include "notification.h"

#include "WindowManager.h"
#include "MainWindowRenderer.h"
#include "MainWindowState.h"

// Gọi lớp trừu tượng đồ họa của bạn từ bài thiết kế trước
#include "OpenGLBackend.h" 
#include "D3D11Backend.h" // Thêm backend mới

// Biến cục bộ thay thế cho globals

#include "OpenGLBackend.h" 

extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}

// AMD GPU
extern "C" {
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}


static VideoInfo& g_videoInfo = GetVideoInfo();
static bool running = true;
void Cleanup() {}

// Hàm định tuyến cập nhật trạng thái tự động thông minh
void RouteWindowStateUpdate(WindowRuntime* runtime) {
    if (!runtime) return;
    if (runtime->style.isMainWindow) {
        UpdateMainWindowState(runtime); // Gọi hàm riêng cho Master + Cập nhật MPV
    } else {
        UpdateWindowStateCommon(runtime); // Gọi hàm chung cho các Sub-window[cite: 18]
    }
}

void HandleWindowRuntimeEvent(WindowRuntime* runtime, const SDL_Event* e, bool& running, bool& hot_key) {
    if (!runtime) return;

    ImGuiContext* imguiCtx = runtime->imguiCtx;
    if (imguiCtx) {
        ImGui::SetCurrentContext(imguiCtx);
    }

    ImGui_ImplSDL2_ProcessEvent(e);

    if (e->type == SDL_MPV_RENDER_UPDATE) {
        if (runtime->mpvSession) runtime->properties.Set<bool>("RenderVideoFlag", true);
    }
    if (e->type == SDL_MPV_EVENT) {
        if (runtime->mpvSession && runtime->mpvSession->GetObserver()) 
            runtime->mpvSession->GetObserver()->ProcessEvents();
    }

    if (e->type == SDL_WINDOWEVENT) {
        switch (e->window.event) {
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_MOVED:
            case SDL_WINDOWEVENT_MAXIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_MINIMIZED:
            case SDL_WINDOWEVENT_HIDDEN:
            case SDL_WINDOWEVENT_SHOWN:
            case SDL_WINDOWEVENT_SIZE_CHANGED: {
                // Sử dụng hàm định tuyến tự động thay vì gọi cứng hàm cũ
                RouteWindowStateUpdate(runtime); 
                break;
            }
            case SDL_WINDOWEVENT_CLOSE: {
                if (runtime->style.isMainWindow) running = false;
                else runtime->state.isClosedPending = true; 
                break;
            }
        }
    }

    hot_key = HandleHotkeys(e, runtime);
    if (hot_key) return;

    if (runtime->state.isVisible) { 
        int mx = -1, my = -1;
        bool isInteraction = false;

        if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
            mx = e->button.x;
            my = e->button.y;
            isInteraction = true;
        }

        if (isInteraction && runtime->style.isMainWindow) { // Chỉ tính toán click chuột vùng video nếu là main window
            if (runtime->properties.Has("Layout")) {
                auto layout = runtime->properties.Get<MainWindowLayout>("Layout");
                SDL_Point mousePos = { mx + layout.WinX, my + layout.WinY };
                
                if (SDL_PointInRect(&mousePos, &layout.videoArea)) {
                    bool showUiVideo = runtime->properties.Get<bool>("ShowUiVideo", true);
                    if (!showUiVideo) {
                        runtime->properties.Set<bool>("ShowUiVideo", true);
                        SDL_ShowCursor(SDL_ENABLE);
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "Global\\MyUniqueApp_MutexID");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        SendArgsToFirstInstance(argc, argv);
        if (hMutex) CloseHandle(hMutex);
        return 0; 
    }

    GetThreadManager().Run(ThreadID::PipeServer, [=]() { PipeServerThread(); });
    SDL_SetMainReady();
    InitConsoleSystem();
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

    ConfigManager::Instance().LoadAll();
    WindowTemplateRegistry registry;
    WindowFactory factory(&registry);
    auto& winManager = WindowManager::GetInstance();
    winManager.Initialize(&factory);

    // Cấu hình mẫu cửa sổ phát video chính
    WindowTemplate mainVideoWinTpl;
    mainVideoWinTpl.name = "Media Video Control";
    mainVideoWinTpl.style.isMainWindow = true; // Thiết lập flag để nhận diện Master Window
    mainVideoWinTpl.style.borderless = true; 
    mainVideoWinTpl.style.titlebar = true;
    mainVideoWinTpl.style.resizable = true;
    mainVideoWinTpl.style.snapEnabled = true;
    mainVideoWinTpl.style.allowhighdpi = true;
    mainVideoWinTpl.style.titleHeight = 28;
    mainVideoWinTpl.style.btnSize = 30;
    mainVideoWinTpl.style.resizeMargin = 8;
    
    mainVideoWinTpl.rendererFactory = []() {
        return std::make_unique<MainWindowRenderer>();
    };
    
    registry.RegisterTemplate("VideoPlayerMain", mainVideoWinTpl);

    // Khởi tạo và nạp thẳng đối tượng thiết lập đồ họa trừu tượng (OpenGL) vào cửa sổ[cite: 20]
    // BẠN CÓ THỂ CHỌN BACKEND Ở ĐÂY
    bool use_d3d11 = false; // Đặt thành true để thử D3D11
    std::unique_ptr<IGraphicsBackend> backend;
    if (use_d3d11) {
        backend = std::make_unique<D3D11Backend>();
    } else {
        backend = std::make_unique<OpenGLBackend>();
    }

    WindowRuntime* mainWin = winManager.CreateNewWindow("VideoPlayerMain", std::move(backend));
    if (!mainWin) {
        Cleanup();
        return 1;
    }

    // Tạo session và chuyển quyền sở hữu cho MPVManager
    auto mpvSession = std::make_unique<MPVSession>("main");
    if (!mpvSession->Init(mainWin)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lỗi", "Không thể khởi tạo MPV Session.", nullptr);
        Cleanup();
        return 1;
    }
    // Đăng ký session với Manager và gán con trỏ thô cho Window
    mainWin->mpvSession = MPVManager::GetInstance().RegisterSession(std::move(mpvSession));
    
  
    // MPVManager giờ đã quản lý session, không cần tìm kiếm nữa
    // Gọi cập nhật trạng thái ban đầu (Sử dụng hàm định tuyến)[cite: 20]
    RouteWindowStateUpdate(mainWin);

    #ifdef RENDER_MPV_THREAD
    #endif

    if (argc >= 2) {
        std::string Url = argv[1];
        CallThread_URLFetch(mainWin, Url, true);
    }

    FrameTimer fpsLimiter(60);
    SDL_Event e;
    bool hot_key = false;

    while (running) {
        fpsLimiter.startFrame();
        CSImGui::UpdateTheme(fpsLimiter.getDeltaTime());
        PlaybackState state = GetPlaybackState();

        // Kiểm tra sự kiện đổi Fullscreen được kích hoạt thông qua Controller/State nội tại
        if (mainWin->properties.Get<bool>("TriggerToggleFullscreen", false)) {
            mainWin->controller->ToggleFullscreen();
            RouteWindowStateUpdate(mainWin); // Sử dụng hàm định tuyến mới[cite: 20]
            mainWin->properties.Set<bool>("TriggerToggleFullscreen", false);
        }

        // Loại bỏ đoạn code `#ifdef RENDER_MPV_THREAD` ở vòng lặp chính này 
        // Vì toàn bộ logic đồng bộ luồng MPV đã được tích hợp gọn gàng bên trong hàm `UpdateMainWindowState`![cite: 20]

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
                break;
            }

            WindowRuntime* targetWin = nullptr;
            if (e.type == SDL_WINDOWEVENT || e.type == SDL_MOUSEBUTTONDOWN || 
                e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION || 
                e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                
                Uint32 winID = 0;
                if (e.type == SDL_WINDOWEVENT) winID = e.window.windowID;
                else if (e.type >= SDL_KEYDOWN && e.type <= SDL_KEYUP) winID = e.key.windowID;
                else if (e.type >= SDL_MOUSEMOTION && e.type <= SDL_MOUSEBUTTONUP) winID = e.motion.windowID;

                if (winID != 0) {
                    SDL_Window* sdlWin = SDL_GetWindowFromID(winID);
                    targetWin = winManager.GetWindowBySDLHandle(sdlWin); 
                }
            }

            if (!targetWin) targetWin = mainWin;

            if (targetWin) {
                HandleWindowRuntimeEvent(targetWin, &e, running, hot_key);
            }
        }

        // Cập nhật các lệnh đang chờ của MPV (ví dụ: delayed seek)
        if (mainWin->mpvSession && mainWin->mpvSession->GetCommander()) 
            mainWin->mpvSession->GetCommander()->Update();
        AudioFilterManager::Instance().UpdateAdaptiveFilters();

        double targetInterval = 1; 
        switch (state) {
            case PlaybackState::Idle:
            case PlaybackState::Paused:
            case PlaybackState::EndOfFile:
                targetInterval = mainWin->properties.Get<bool>("ShowUiVideo", true) ? 2 : 30;
                break;
            case PlaybackState::Loading:
            case PlaybackState::Seeking:
                targetInterval = 2.5;
                break;
            case PlaybackState::Playing:
                targetInterval = 1; 
                break;
        }

        if (IsAnyPopupOpen() || hot_key) {
            if (targetInterval > 2) targetInterval = 2; 
        }

        fpsLimiter.set_ev_frame(targetInterval);

        // Render toàn bộ các cửa sổ đang quản lý thông qua lớp đồ họa Backend trừu tượng
        if (fpsLimiter.isEvery()) {
            for (auto& [id, windowInstance] : winManager) {
                if (!windowInstance->state.isVisible) continue; 

                // Tự động chuyển đổi ngữ cảnh cửa sổ
                //ImGuiContext* ctxOfWindow = windowInstance->properties.Get<ImGuiContext*>("ImGuiCtx");
                ImGuiContext* ctxOfWindow = windowInstance->imguiCtx;
                if (ctxOfWindow) ImGui::SetCurrentContext(ctxOfWindow);

                if (windowInstance->graphicsBackend && windowInstance->renderer) {
                    // Sử dụng interface Backend thay vì gọi trực tiếp lệnh GL thô sơ[cite: 20]
                    windowInstance->graphicsBackend->BeginFrame(windowInstance->sdlWindow);
                    windowInstance->renderer->RenderUI(windowInstance.get());
                    windowInstance->graphicsBackend->EndFrame(windowInstance->sdlWindow);
                }
            }
        }

        g_videoInfo.currentFPS = fpsLimiter.getFPS();
        fpsLimiter.endFrame();
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    
    winManager.DestroyWindow(mainWin->id);
    // Cleanup(); // CleanupMPV is now handled by ~MPVSession
    return 0;
}