
#include "mpv/mpv_settings.h"
#include "mpv/mpv_ui_settings.h"
#include "mpv_controller.h"
#include "globals.h"
#include "utils.h"
#include "services.h"
#include "thread_manager.h"
//#undef RATE_LIMITED_COUT
//#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>
#include <mutex>
#include <algorithm>

void CallThread_URLFetch(const std::string& Url , bool playNow ,  const std::string& title ,const std::string& format_id) {

    GetThreadManager().Run(ThreadID::URLFetch, [=]() {

        playImmediately = playNow;  
        url_play = Url;
        
        v_Settings.selectedResolution = v_Settings.selectedFormat + "+" + v_Settings.selectedAudio;
        int result = PlayVideo(mpv, Url, (!format_id.empty() ? format_id : v_Settings.selectedResolution ), title);
    });
}

void StartServiceThread() {

    //std::thread([]() {
    //    EnsureResolutionServiceRunning();
    //}).detach();
    std::thread([]() {
        EnsureYouTubeServiceRunning();
    }).detach();
    std::thread([]() {
        EnsureKeyManagerServiceRunning();
    }).detach();
}

