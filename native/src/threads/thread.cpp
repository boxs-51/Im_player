
#include <mpv/mpv_settings.h>
#include <mpv/mpv_ui_settings.h>
#include <mpv_controller.h>

#include <threads/thread.h>

#include "utils.h"
#include <threads/thread_manager.h>

#include <log.h>
#include <mutex>
#include <algorithm>

void CallThread_URLFetch(const std::string& Url , bool playNow ,  const std::string& title ,const std::string& format_id) {

    GetThreadManager().Run(ThreadID::URLFetch, [=]() {

        playImmediately = playNow;  
        v_Settings.selectedResolution = v_Settings.selectedFormat + "+" + v_Settings.selectedAudio;
        int result = PlayVideo(mpv.mpv, Url, (!format_id.empty() ? format_id : v_Settings.selectedResolution ), title);
    });
}



