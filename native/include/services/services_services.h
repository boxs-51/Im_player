#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <windows.h>

struct Services {
    PROCESS_INFORMATION g_KeyServiceProcess = {0};
    PROCESS_INFORMATION g_ResolutionServiceProcess = {0};
    PROCESS_INFORMATION g_YouTubeServiceProcess = {0};

    std::mutex g_keyServiceMutex;
    std::mutex g_youtubeServiceMutex;
    std::mutex g_serviceMutex;

    std::condition_variable g_keyServiceCv;
    std::condition_variable g_youtubeServiceCv;
    std::condition_variable g_serviceCv;

    std::atomic<bool> g_KeyServiceStarted {false};
    std::atomic<bool> g_YouTubeServiceRunning {false};
    std::atomic<bool> g_YouTubeServiceStarted{false};
    std::atomic<bool> g_ServiceStarted{false};
    std::atomic<bool> g_ResolutionServiceRunning{false};
};
// Khởi động tất cả dịch vụ nền
void StartRuntimeServices();
void WaitForServicesReady();
void StopService();