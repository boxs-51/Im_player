#include "backend.h"
#include "vid.h"

#include <thread>

Services services;

void WaitForServicesReady() {
    std::unique_lock<std::mutex> lock2(services.g_youtubeServiceMutex);
    services.g_youtubeServiceCv.wait(lock2, []() { return services.g_YouTubeServiceStarted.load(); });
}
void StopService(){
    StopYouTubeService(services) ;
}
void StartRuntimeServices() {
    std::thread([]() {
        EnsureYouTubeServiceRunning(services);
    }).detach();
}
