#include "services/services_services.h"
#include "services/services_youtube.h" 
#include "services/services_key_manager.h" 

#include <thread>
#include <iostream>
#include <chrono>

Services services;

void WaitForServicesReady() {
    //std::unique_lock<std::mutex> lock1(g_serviceMutex);
    std::unique_lock<std::mutex> lock2(services.g_youtubeServiceMutex);
    std::unique_lock<std::mutex> lock3(services.g_keyServiceMutex);

    //g_serviceCv.wait(lock1, []() { return g_ServiceStarted.load(); });
    services.g_youtubeServiceCv.wait(lock2, []() { return services.g_YouTubeServiceStarted.load(); });
    services.g_keyServiceCv.wait(lock3, []() { return services.g_KeyServiceStarted.load(); });

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
    StopYouTubeService(services) ;
    StopKeyManagerService(services);
}
void StartRuntimeServices() {
    std::thread([]() {
        EnsureYouTubeServiceRunning(services);
    }).detach();
    std::thread([]() {
        EnsureKeyManagerServiceRunning(services);
    }).detach();
}
