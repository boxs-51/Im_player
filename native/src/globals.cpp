
// globals.cpp
#include "globals.h"
#include "popup.h"
#include <string>
#include <vector>
#include "reusable_window.h"
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

const std::string SETTINGS_PATH_COMMOM    = AutoPath<std::string>("%ROOT%", "data","settings_common.json");
const std::string SETTINGS_PATH_VIDEO     = AutoPath<std::string>("%ROOT%", "data","settings_video.json");
const std::string SETTINGS_PATH_POPUP_URL = AutoPath<std::string>("%ROOT%", "data","popup_data.json");


std::vector<std::string> g_keywords;

std::mutex g_mutex;
std::string g_nextPageToken;
std::string g_searchQuery;

bool Disabehotkey = false;
bool playImmediately = true;
bool audio_Theme = false;

double pendingSeekTime = -1.0;

Uint32 lastInteractionTime = 0;

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

std::vector<ReusableWindow*> allReusableWindows = {
    &SidebarWindow,
};


ReusableWindow g_urlWindow;
ReusableWindow SidebarWindow;
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

void RenderAllPopups(mpv_handle* mpv) {
    // Nếu vẫn còn dùng popup cũ (dạng ReusablePopup)
    if (Popup_Url.IsOpen()){
        RenderPopupOverlay_Url(mpv);
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



