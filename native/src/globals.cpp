
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

#include "mpv/mpv_settings.h"

bool g_consoleAttached = false;
bool g_consoleWindowCreated = false;
SeekingData dataseek;
UiWindowsState uiState;

const std::string SETTINGS_PATH_COMMOM    = AutoPath<std::string>("%ROOT%", "data","settings_common.json");
const std::string SETTINGS_PATH_VIDEO     = AutoPath<std::string>("%ROOT%", "data","settings_video.json");
const std::string SETTINGS_PATH_POPUP_URL = AutoPath<std::string>("%ROOT%", "data","popup_data.json");


std::atomic<bool> g_KeyServiceStarted = false;
std::mutex g_keyServiceMutex;
std::condition_variable g_keyServiceCv;
PROCESS_INFORMATION g_KeyServiceProcess;

bool formatChangeConfirmed = false;
int previousPlaylistIndex = -1;
std::vector<std::string> g_keywords;
double g_lastKeywordFetch = 0.0;

PROCESS_INFORMATION g_ResolutionServiceProcess = {0};
PROCESS_INFORMATION g_YouTubeServiceProcess = {0};
std::mutex g_youtubeServiceMutex;
std::condition_variable g_youtubeServiceCv;
std::atomic<bool> g_YouTubeServiceRunning = false;
std::atomic<bool> g_YouTubeServiceStarted = false;

std::mutex g_mutex;
std::string g_nextPageToken;
std::string g_searchQuery;

int reset_FPS_int = 0 ;
int g_CurrentIndex = -1;
int selectedAudioIndex = -1;


bool Disabehotkey = false;
bool playImmediately = true;
bool tool_video = false;
bool cookies = false;
bool audio_Theme = false;
bool reset_FPS_bool = false;
bool g_hasload = false;
bool urlConfirmed = false;
bool loadingVideoformpopup = false;
bool showIOCHSettings = false;
bool hasRenderedSomething = false;  
bool needRender = false;
bool render_video = false;
bool render_ui_video = false;
bool render_load = false;
bool mpvUpdatePaused = false;
bool g_isChangingVideo = false;
bool g_wasPlayingBeforeSeek = false;
bool g_isSeekPending = false;
bool g_WindowVisible = true;
//bool g_RequestToggleFullscreen = false;
bool shouldSeekAfterResolutionChange = false;
bool mpv_player_ready = true;
bool rendee_ilde =false;
bool show_ui_video = false;
bool hasRenderedIdleFrame = false;
bool hasRenderedPausedFrame = false;
bool pausedFrameValid = false;
bool autoScaleVideo = true;
bool fpsInited  = false;
bool file_local = false;
bool is_live = false;

int volume = 100;
int targetFPS = 60;
int pause_render_ui_video =  0  ;
int pause_render_video = 0  ;
int render_ilde_full = 0;
int relay_render_loading = 0;
int hiddenDelay = 0; 
int maxHiddenDelay = 5000;


double pendingSeekTime = -1.0;

float g_seekTargetTime = -1.0;
float g_seekingIconFade = 0.0f;

Uint32 mpvPollDelayWhenHidden = 200; 
Uint32 Relay_Ilde = 0;
Uint32 invisibleStartTime = 0;
Uint32 frameDelay = 1000 / targetFPS;
Uint32 down_FPS_visible = 0;
Uint32 frameStart;
Uint32 frameTime;
Uint32 g_lastSeekRequestTime = 0;
Uint32 delayProcessEvents = 0;
Uint32 lastInteractionTime = 0;
Uint32 lastUpdateTime = 0;

Uint32 off_ProcessEvents = 0;

std::wstring Url;
std::wstring confirmed_url;

std::string url_play = "";

std::function<void(int)> g_OnVideoSelected;
std::queue<std::string> g_errorQueue;
std::mutex g_errorMutex;
std::mutex g_playlistMutex;



std::thread g_SidebarThread;

ReusablePopup Popup_Url;
ReusablePopup videoInfoPopup;
ReusablePopup SettingPopup;
ReusablePopup SidarBarPopup;

GLuint pausedFrameTexture = 0;
bool useGlobalVideoSize = true;


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
int currentIndex;

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

std::mutex g_ResolutionCacheMutex;
std::atomic<bool> g_ServiceStarted{false};
std::mutex g_serviceMutex;
std::condition_variable g_serviceCv;
std::atomic<bool> g_ResolutionServiceRunning{false};
std::string videoType = "unknown";

