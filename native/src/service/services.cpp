#include "services.h"
#include "globals.h"
#include "utils.h"
#include "thread.h"

#include <iostream>
#include <chrono>

void WaitForServicesReady() {
    //std::unique_lock<std::mutex> lock1(g_serviceMutex);
    std::unique_lock<std::mutex> lock2(g_youtubeServiceMutex);
    std::unique_lock<std::mutex> lock3(g_keyServiceMutex);

    //g_serviceCv.wait(lock1, []() { return g_ServiceStarted.load(); });
    g_youtubeServiceCv.wait(lock2, []() { return g_YouTubeServiceStarted.load(); });
    g_keyServiceCv.wait(lock3, []() { return g_KeyServiceStarted.load(); });

    // Hoặc nếu muốn chờ cả hai cùng lúc:
    /*
    std::unique_lock<std::mutex> lock(g_serviceMutex);
    g_serviceCv.wait(lock, []() {
        return g_ServiceStarted.load() && g_YouTubeServiceStarted.load();
    });
    */
}
void StopService(){
    //StopResolutionService();
    StopYouTubeService() ;
    StopKeyManagerService();
}
void StartRuntimeServices() {

    StartServiceThread();
}
