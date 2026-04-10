
// globals.cpp
#include "globals.h"
#include "popup.h"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <queue>
#include <map>

#include <mpv/mpv_custom_ui.h>
#include <mpv/mpv_settings.h>
#include <mpv/render_gl.h>

ThemeTransition GTrans;
ThemeColors GTheme;
SeekingData dataseek;
UiWindowsState uiState;
MPV mpv;

std::map<ThemeType, ThemeColors> ThemeLibrary;

std::vector<std::string> g_keywords;

std::mutex g_mutex;
std::string g_nextPageToken;
std::string g_searchQuery;

bool ToggleFullscreen = false;
bool Disabehotkey = false;
bool playImmediately = true;
bool audio_Theme = false;

double pendingSeekTime = -1.0;

Uint64 lastInteractionTime = 0;

std::queue<std::string> g_errorQueue;

ReusablePopup Popup_Url;
ReusablePopup videoInfoPopup;
ReusablePopup SettingPopup;
ReusablePopup SidarBarPopup;

std::vector<ReusablePopup*> allReusablePopups = {
    &Popup_Url,
    &videoInfoPopup,
    &SettingPopup,
    &SidarBarPopup
};



std::vector<std::wstring> playlist;

bool IsAnyPopupOpen() {
    for (auto* popup :  allReusablePopups) {
        if (popup->IsOpen()) return true;
    }
    return false;
}


std::vector<ReusablePopup*>& GetAllPopups() {
    return allReusablePopups;
}

void RenderAllPopups() {
    // Nếu vẫn còn dùng popup cũ (dạng ReusablePopup)
    if (Popup_Url.IsOpen()){
        RenderPopupOverlay_Url();
    }
    if (videoInfoPopup.IsOpen()) {
        RenderVideoInfoPopup(); 
    }
    if (SettingPopup.IsOpen()) {
        RenderSettingPopup(); 
    }
    if (SidarBarPopup.IsOpen()) {
        RenderSidarBarPopup();
    }
}
void OffPopup(){
    Popup_Url.Close();
    videoInfoPopup.Close();
    SidarBarPopup.Close();
    SettingPopup.Close();
}



