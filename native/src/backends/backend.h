#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <windows.h>

struct Services {
    PROCESS_INFORMATION g_YouTubeServiceProcess = {0};
    std::mutex g_youtubeServiceMutex;
    std::condition_variable g_youtubeServiceCv;
    std::atomic<bool> g_YouTubeServiceRunning {false};
    std::atomic<bool> g_YouTubeServiceStarted{false};
};

extern Services services;

void WaitForServicesReady();
void StopService();
void StartRuntimeServices();
