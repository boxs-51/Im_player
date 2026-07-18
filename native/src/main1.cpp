// main.cpp
#include <SDL.h>
#include "WindowTemplate.h"
#include "WindowFactory.h"
#include "WindowManager.h"
#include "MainWindowRenderer.h"
#include "MainWindowState.h"
#include "main.h"
#include "globals.h"
#include "utils.h"
#include <windows.h>
#include <csignal>
#include <gui/gui.h>
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <mpv/mpv_ui.h>
#include <mpv/mpv_controller.h>
#include <mpv/mpv_render_video.h>
#include <mpv/mpv_ui_settings.h>
#include <mpv/render_gl.h>
#include <mpv/mpv_data.h>

#include <mpv/audio/filter/af_m.h>

#include "settings_manager.h"
#include <popup/popup.h>

#include "utils.h"
#include "hotkey_handler.h"
#include "globals.h"

#include <backends/backend.h>

#include <threads/thread_manager.h>
#include <log.h>
#include "main.h"
#include "notification.h"

static VideoInfo& g_videoInfo = GetVideoInfo();
static DragResizeState& g_DragResizeState = GetDragResizeState();
static bool running = true;
void Cleanup() {

}


void HandleWindowRuntimeEvent(WindowRuntime* runtime, const SDL_Event* e, bool& running, bool& hot_key) {
    if (!runtime) return;

    // 1. Tự động chuyển đổi Context ImGui chính xác cho cửa sổ này
    ImGuiContext* imguiCtx = runtime->properties.Get<ImGuiContext*>("ImGuiCtx");
    if (imguiCtx) {
        ImGui::SetCurrentContext(imguiCtx);
    }

    // 2. Để ImGui tiêu thụ sự kiện trước
    ImGui_ImplSDL2_ProcessEvent(e);
    
    // 3. Xử lý các sự kiện Core liên kết với MPV (chỉ khi runtime này có chứa MPV)
    if (e->type == SDL_MPV_RENDER_UPDATE) {
        runtime->properties.Set<bool>("RenderVideoFlag", true);
    }
    if (e->type == SDL_MPV_EVENT) {
        ProcessMPVEvents(mpv.mpv);
    }

    // 4. Xử lý thay đổi hình học / trạng thái vật lý của riêng cửa sổ này
    if (e->type == SDL_WINDOWEVENT) {
        switch (e->window.event) {
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_MOVED:
            case SDL_WINDOWEVENT_MAXIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_SIZE_CHANGED: {
                // Gọi hàm cập nhật state nội tại mà chúng ta vừa thiết kế
                UpdateWindowState(runtime); 
                break;
            }
            case SDL_WINDOWEVENT_CLOSE: {
                // Cho phép đóng cửa sổ phụ mà không sập app, nếu là mainWin thì tắt app
                if (runtime->style.isMainWindow) running = false;
                else runtime->state.isClosedPending = true; // Flag để winManager xóa sau
                break;
            }
        }
    }

    // 5. Xử lý Hotkeys toàn cục (nếu có)
    hot_key = HandleHotkeys(e, mpv.mpv);
    if (hot_key) return;

    // 6. Xử lý tương tác Chuột cục bộ dựa trên layout riêng của chính cửa sổ này
    if (runtime->state.isVisible) { // Đọc trực tiếp từ state đã sync
        int mx = -1, my = -1;
        bool isInteraction = false;

        if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
            mx = e->button.x;
            my = e->button.y;
            isInteraction = true;
        }

        if (isInteraction) {
            // Đọc layout động từ PropertyBag của chính instance này
            if (runtime->properties.Has("Layout")) {
                auto layout = runtime->properties.Get<WindowLayout>("Layout");
                
                // Chuyển đổi sang tọa độ Screen-space tương ứng với cửa sổ này
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
    // 1. Kiểm tra Single Instance Mutex cũ độc lập[cite: 5]
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

    // Đăng ký System signals
    std::signal(SIGSEGV, SignalHandler);
    std::signal(SIGABRT, SignalHandler);
    std::signal(SIGFPE, SignalHandler);
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }

    // Thiết lập cấu hình OpenGL[cite: 5]
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // ========================================================
    // KHỞI TẠO KIẾN TRÚC WINDOW MỚI (TẬP TRUNG)
    // ========================================================
    WindowTemplateRegistry registry;
    WindowFactory factory(&registry);
    WindowManager winManager(&factory);

    // Cấu hình mẫu cửa sổ phát video chính
    WindowTemplate mainVideoWinTpl;
    mainVideoWinTpl.name = "Media Video Control";
    mainVideoWinTpl.style.borderless = true; // Dùng Custom Titlebar[cite: 5]
    mainVideoWinTpl.style.titlebar = true;
    mainVideoWinTpl.style.resizable = true;
    mainVideoWinTpl.style.snapEnabled = true;
    mainVideoWinTpl.style.titleHeight = 28;
    mainVideoWinTpl.style.btnSize = 30;
    mainVideoWinTpl.style.resizeMargin = 8;
    
    // Gán Factory cấp phát Renderer động cho Template
    mainVideoWinTpl.rendererFactory = []() {
        return std::make_unique<MainWindowRenderer>();
    };
    
    // Đăng ký mẫu vào Registry
    registry.RegisterTemplate("VideoPlayerMain", mainVideoWinTpl);

    // Khởi tạo thực thể cửa sổ thông qua Quản lý tập trung
    WindowRuntime* mainWin = winManager.CreateNewWindow("VideoPlayerMain");
    if (!mainWin) {
        Cleanup();
        return 1;
    }

    // Khởi tạo Context MPV liên kết[cite: 5]
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

    ConfigManager::Instance().LoadAll();
    CSImGui::InitThemeLibrary(ConfigManager::Instance().GetCommonSettings().themetype);

    UpdateWindowState(mainWin);

    #ifdef RENDER_MPV_THREAD
    StartMPVRenderThread(mainWin);
    #endif

    if (argc >= 2) {
        std::string Url = argv[1];
        CallThread_URLFetch(Url, true);
    }

    FrameTimer fpsLimiter(60);
    SDL_Event e;
    bool hot_key = false;

    
    // ========================================================
    // MAIN LOOP (ỨNG DỤNG ĐA CỬA SỔ)
    // ========================================================
    while (running) {
        fpsLimiter.startFrame();
        CSImGui::UpdateTheme(fpsLimiter.getDeltaTime());
        PlaybackState state = GetPlaybackState();

        // 1. Kiểm tra sự kiện đổi Fullscreen được kích hoạt thông qua Controller/State nội tại
        if (mainWin->properties.Get<bool>("TriggerToggleFullscreen", false)) {
            mainWin->controller->ToggleFullscreen();
            UpdateWindowState(mainWin);
            mainWin->properties.Set<bool>("TriggerToggleFullscreen", false);
        }
        #ifdef RENDER_MPV_THREAD
            // 1. Đồng bộ trạng thái Ẩn/Hiện vật lý sang cho Luồng Render MPV phụ
            renderThread.g_WindowVisible.store(mainWin->state.isVisible, std::memory_order_relaxed);

            // 2. Đồng bộ kích thước hình học mới nếu người dùng đang kéo co giãn cửa sổ
            if (mainWin->properties.Contains("Layout")) {
                auto layout = mainWin->properties.Get<MainWindowLayout>("Layout");
                
                std::lock_guard<std::mutex> lock(renderThread.mtx);
                if (renderThread.surface.drawW != (int)layout.VideoSize.x || 
                    renderThread.surface.drawH != (int)layout.VideoSize.y) {
                    
                    renderThread.surface.newW = (int)layout.VideoSize.x;
                    renderThread.surface.newH = (int)layout.VideoSize.y;
                    renderThread.surface.needResize = true;
                }
            }
        #endif

        // 2. Phân phối và xử lý sự kiện
        while (SDL_PollEvent(&e)) {
            // 1. Xử lý các sự kiện toàn cục hệ thống trước
            if (e.type == SDL_QUIT) {
                running = false;
                break;
            }

            // 2. Tìm WindowRuntime tương ứng với sự kiện
            WindowRuntime* targetWin = nullptr;
            
            // Nếu là sự kiện liên quan đến Cửa sổ (Resize, di chuyển, chuột, phím)
            if (e.type == SDL_WINDOWEVENT || e.type == SDL_MOUSEBUTTONDOWN || 
                e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION || 
                e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                
                Uint32 winID = 0;
                if (e.type == SDL_WINDOWEVENT) {
                    winID = e.window.windowID;
                } else if (e.type >= SDL_KEYDOWN && e.type <= SDL_KEYUP) {
                    winID = e.key.windowID;
                } else if (e.type >= SDL_MOUSEMOTION && e.type <= SDL_MOUSEBUTTONUP) {
                    winID = e.motion.windowID; // windowID nằm cùng vị trí cấu trúc cho motion/button
                }

                // Tìm con trỏ WindowRuntime* từ windowID của SDL
                if (winID != 0) {
                    SDL_Window* sdlWin = SDL_GetWindowFromID(winID);
                    targetWin = winManager.GetWindowBySDLHandle(sdlWin); // Bạn cần thêm hàm tiện ích này vào WindowManager
                }
            }

            // Nếu không tìm thấy cửa sổ cụ thể (hoặc là sự kiện custom như MPV), mặc định phân phối cho mainWin
            if (!targetWin) {
                targetWin = mainWin;
            }

            // 3. Đẩy sự kiện vào hàm xử lý đã chuẩn hóa của cửa sổ đó
            if (targetWin) {
                HandleWindowRuntimeEvent(targetWin, &e, running, hot_key);
            }
        }
        mpv_update_seek_pending(mpv.mpv);
        AudioFilterManager::Instance().UpdateAdaptiveFilters();

        // 4. Tính toán tốc độ làm mới thông minh (Adaptive Refresh Rate) cho Window
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

        // 5. Duyệt vẽ toàn bộ các cửa sổ đang có trong Registry quản lý
        if (fpsLimiter.isEvery()) {
            for (auto& [id, windowInstance] : winManager) {
                if (!windowInstance->state.isVisible) continue; // Bỏ qua nếu cửa sổ bị ẩn

                // Chuyển đổi ngữ cảnh ImGui chính xác cho cửa sổ đang vẽ
                ImGuiContext* ctxOfWindow = windowInstance->properties.Get<ImGuiContext*>("ImGuiCtx");
                if (ctxOfWindow) ImGui::SetCurrentContext(ctxOfWindow);

                if (windowInstance->renderer) {
                    windowInstance->renderer->BeginFrame();
                    windowInstance->renderer->RenderUI(windowInstance.get());
                    windowInstance->renderer->EndFrame();
                }
                
                SDL_GL_SwapWindow(windowInstance->sdlWindow);
            }
        }

        g_videoInfo.currentFPS = fpsLimiter.getFPS();
        fpsLimiter.endFrame();
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    
    // Thu dọn toàn bộ các Window đang hoạt động trước khi tắt app
    winManager.DestroyWindow(mainWin->id);
    Cleanup();
    return 0;
}