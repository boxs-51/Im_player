// main.cpp
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include <SDL.h>
#include <windows.h>
#include <csignal>
#include "utils.h"

#include "gui/gui.h"

#include "player/session/PlayerManager.h"

#include "player/audio/filter/af_m.h"

#include "settings_manager.h"
#include "popup/popup.h"
#include "hotkey_handler.h"
#include <backends/backend.h>
#include <threads/thread_manager.h>
#include <log.h>
#include "notification.h"

#include "windows/main/MainWindowRenderer.h"
#include "windows/sub/MockSubWindowRenderer.h" // Thêm include cho renderer mới

#include "windows/WindowManager.h"
#include "FontManager.h"
#include "windows/UpdateWindowState.h"
#include "windows/WindowFactory.h"
#include "windows/UIRenderThread.h"
#include "windows/WindowSharedGroup.h"
#include "common/Exception.h"
#include "windows/WindowTemplateBuilder.h"
#include "EventQueue.h"

// Gọi lớp trừu tượng đồ họa của bạn từ bài thiết kế trước
#include "OpenGLBackend.h"
#include "D3D11Backend.h" // Thêm backend mới

// Định nghĩa biến thread-local cho context của ImGui
/*
// Bật tính năng thread-local storage cho context của ImGui
#define GImGui MyImGuiTLS
struct ImGuiContext;
extern thread_local ImGuiContext* MyImGuiTLS;
*/
thread_local ImGuiContext *MyImGuiTLS = NULL;

extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}

// AMD GPU
extern "C"
{
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static bool running = true;
void Cleanup() {}



int main(int argc, char **argv)
{

    HANDLE hMutex = CreateMutexA(NULL, TRUE, "Global\\MyUniqueApp_MutexID");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        SendArgsToFirstInstance(argc, argv);
        if (hMutex)
            CloseHandle(hMutex);
        return 0;
    }

    try
    {

        GetThreadManager().Run("PipeServer", [=]()
                               { PipeServerThread(); });
        SDL_SetMainReady();
        InitConsoleSystem();
        StartRuntimeServices();
        std::set_terminate(TerminateHandler);

        std::signal(SIGSEGV, SignalHandler);
        std::signal(SIGABRT, SignalHandler);
        std::signal(SIGFPE, SignalHandler);
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        {
            SDL_Log("SDL_Init Error: %s", SDL_GetError());
            return 1;
        }

        ConfigManager::Instance().LoadAll();

        WindowTemplateRegistry registry;
        WindowFactory factory(&registry);

        FontManager::Instance().ScanDirectories({ "assets/fonts", "C:/Windows/Fonts" });

        // Khởi tạo WindowManager
        auto &winManager = WindowManager::GetInstance();
        winManager.Initialize(&factory);

        // Sử dụng WindowTemplateBuilder để đăng ký template một cách gọn gàng
        WindowTemplateBuilder()
            .WithName("Media Video Control")
            .WithStyle([](WindowStyle &s)
                       {
            s.isMainWindow = true;
            s.borderless = true;
            s.create_mpv = true; })
            .WithState([](WindowState &s)
                       {
            s.geometry.minWidth = 720;
            s.geometry.minHeight = 360; })
            .WithBackend<OpenGLBackend>()
            .WithLoop(90)
            .WithRenderer<MainWindowRenderer>()
            .Register(registry, "VideoPlayerMain");

        // --- Đăng ký template cho cửa sổ phụ mô phỏng ---
        WindowTemplateBuilder()
            .WithName("Mock Sub-Window")
            .WithStyle([](WindowStyle &s)
                       { s.borderless = true; })
            .WithState([](WindowState &s)
                       {
            s.geometry.width = 400;
            s.geometry.height = 300; })
            .WithRenderer<MockSubWindowRenderer>()
            .WithBackend<OpenGLBackend>()
            .WithLoop(30)
            .Register(registry, "MockSubWindow");

        // Khởi tạo và nạp thẳng đối tượng thiết lập đồ họa trừu tượng (OpenGL) vào cửa sổ[cite: 20]

        winManager.QueueCreateWindow("VideoPlayerMain");
        winManager.ProcessCreationQueue(); // Xử lý ngay để có mainWin
        WindowRuntime *mainWin = winManager.GetMainWindow();
        if (!mainWin)
        { // Kiểm tra xem mainWin đã được tạo thành công chưa
            // Dọn dẹp trước khi thoát
            if (hMutex)
            {
                ReleaseMutex(hMutex);
                CloseHandle(hMutex);
            }
            return 1;
        }


#ifdef RENDER_MPV_THREAD
#endif

        if (argc >= 2)
        {
            std::string Url = argv[1];
            if(auto* session = mainWin->resource.GetPlayerSession())
            if (auto *commander = mainWin->resource.GetPlayerSession()->GetCommander())
                commander->LoadFile(Url);
        }

        FrameTimer mainloop(120);
        SDL_Event e;

        // Danh sách các cửa sổ cần đóng sau khi vòng lặp sự kiện kết thúc
        std::vector<WindowId> windowsToClose;
        const WindowId mainWinId = mainWin ? mainWin->info.id : 0;

        while (running)
        {
            mainloop.startFrame();
            CSImGui::UpdateTheme(mainloop.getDeltaTime());

            // Xử lý các yêu cầu tạo cửa sổ từ các luồng khác
            winManager.ProcessCreationQueue();

            while (SDL_PollEvent(&e))
            {
                if (e.type == SDL_QUIT)
                {
                    running = false;
                    // Khi nhận SDL_QUIT, yêu cầu đóng tất cả cửa sổ
                    winManager.ForEachWindow([](WindowRuntime *window)
                    { 
                        {
                            std::lock_guard<std::mutex> lock(window->stateMutex);
                            window->state.runtime.isClosedPending = true; 
                        }
                    });
                    break;
                }

                // Sử dụng hàm mới để tìm cửa sổ đích một cách gọn gàng
                WindowRuntime *targetWin = winManager.GetWindowFromEvent(&e);

                // Nếu sự kiện không thuộc về cửa sổ nào cụ thể (ví dụ: joystick),
                // có thể gửi nó đến cửa sổ chính để xử lý hotkey toàn cục.
                if (!targetWin)
                {
                    targetWin = mainWin;
                }

                if (targetWin)
                {
                    HandleWindowRuntimeEvent(targetWin, &e);
                }
            }

            windowsToClose.clear();
            winManager.ForEachWindow([&windowsToClose](WindowRuntime *window)
            {
                // Kiểm tra sự kiện đổi Fullscreen được kích hoạt thông qua Controller/State nội tại
                if (window->properties.GetValue<bool>("TriggerToggleFullscreen", false))
                {
                    window->controller->ToggleFullscreen();
                    //UpdateWindowState(window);
                    window->properties.Set<bool>("TriggerToggleFullscreen", false);
                }

                AdjustWindowFrameRates(window);

                if (auto* session = window->resource.GetPlayerSession()) {
                    // Cập nhật các lệnh đang chờ của MPV (ví dụ: delayed seek)
                    if (auto* commander = session->GetCommander())
                        commander->Update();

                    if (auto* audiofillter = session->GetAudioFilterManager())
                        audiofillter->UpdateAdaptiveFilters();
                }

                // Yêu cầu render cho tất cả các cửa sổ đang hiển thị
                // Mỗi cửa sổ có luồng render riêng, yêu cầu render nếu nó hiển thị
                bool isClosedPending = false;
                bool isVisible = false;
                {
                    std::lock_guard<std::mutex> lock(window->stateMutex);
                    isClosedPending = window->state.runtime.isClosedPending;
                    isVisible = window->state.display.isVisible;
                }
                if (window && window->resource.uiRenderThread && isVisible)
                {
                    window->resource.uiRenderThread->RequestRender();
                }

                if (isClosedPending) {
                    windowsToClose.push_back(window->info.id);
                    isClosedPending = false;
                }
                {
                    std::lock_guard<std::mutex> lock(window->stateMutex);
                    window->state.runtime.isClosedPending = isClosedPending;
                }
            });

            // --- Giai đoạn dọn dẹp sau vòng lặp sự kiện ---
            if (!windowsToClose.empty())
            {
                for (WindowId idToClose : windowsToClose)
                {
                    WindowRuntime *winToClose = winManager.GetWindowById(idToClose);
                    if (!winToClose)
                        continue;

                    // Nếu là cửa sổ chính, hoặc cửa sổ không được thiết kế để ẩn/hiện -> Hủy
                    if (winToClose->style.isMainWindow || idToClose == mainWinId /* || some_other_condition */)
                    {
                        running = false;
                    }
                    else
                    {
                        // Nếu là cửa sổ phụ -> Chỉ ẩn đi
                        winManager.HideWindow(idToClose);
                    }
                }
            }

            main_loop_rate = mainloop.getFPS();
            mainloop.endFrame();
        }

        if (hMutex)
        {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }

        std::vector<WindowId> allIds;
        winManager.ForEachWindow([&allIds](WindowRuntime *window)
                                 { allIds.push_back(window->info.id); });
        for (WindowId id : allIds)
        {
            winManager.DestroyWindow(id);
        }
        FontManager::Instance().Shutdown();
        // Cleanup(); // CleanupMPV is now handled by ~PlayerSession
        return 0;
    }
    catch (const AppException &e)
    {
        SDL_Log("Caught AppException: %s", e.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Application Error", e.what(), nullptr);
        if (hMutex)
        {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        return 1;
    }
    catch (const std::exception &e)
    {
        SDL_Log("Caught std::exception: %s", e.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unhandled Exception", e.what(), nullptr);
        if (hMutex)
        {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        return 1;
    }
    catch (...)
    {
        SDL_Log("Caught unknown exception.");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unknown Error", "An unknown error occurred.", nullptr);
        if (hMutex)
        {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        return 1;
    }
}