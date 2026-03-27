// globals.h
#pragma once
#include "reusable_popup.h"
#include "utils.h"
#include "reusable_window.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "mpv_settings.h"
#include "mpv_controller.h" 
#include "services.h"
#include "stb_image.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <GL/gl3w.h> 
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>
#include <string>
#include <mpv/client.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <algorithm>
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <condition_variable>

struct SeekingData{
    float forward = 0.0f;
    float timer =0.0f;
    float g_seekingIconFade =0.0f;
    float pulse = 0.0f;
    float alpha = 0.0f;

    bool g_isSeeking = false;
};

extern SeekingData Seekingdata;

struct UiWindowsState {
    bool show_demo = false;
    bool show_style = false;
    bool show_metrics = false;
    bool show_log = false;
    bool show_settings = false;
};
extern UiWindowsState uiState;
extern const std::string SETTINGS_PATH_COMMOM  ;
extern const std::string SETTINGS_PATH_VIDEO   ;
extern const std::string SETTINGS_PATH_POPUP_URL ;

extern int WinX_SDL;
extern int WinY_SDL;
extern int WinW_SDL;
extern int WinH_SDL;

extern std::atomic<bool> g_KeyServiceStarted;
extern std::mutex g_keyServiceMutex;
extern std::condition_variable g_keyServiceCv;
extern PROCESS_INFORMATION g_KeyServiceProcess;

extern bool formatChangeConfirmed;
extern int previousPlaylistIndex;
extern std::vector<std::string> g_keywords;
extern double g_lastKeywordFetch;

extern std::mutex g_mutex;
extern std::string g_nextPageToken;
extern std::string g_searchQuery;

extern PROCESS_INFORMATION g_YouTubeServiceProcess;
extern std::mutex g_youtubeServiceMutex;
extern std::condition_variable g_youtubeServiceCv;
extern std::atomic<bool> g_YouTubeServiceRunning;
extern std::atomic<bool> g_YouTubeServiceStarted;

extern PROCESS_INFORMATION g_ResolutionServiceProcess;

extern std::function<void(int)> g_OnVideoSelected;
extern std::string videoType;
extern std::string url_play;

extern std::atomic<bool> g_ResolutionServiceRunning;
extern std::atomic<bool> g_ServiceStarted;


extern std::mutex g_serviceMutex;
extern std::mutex g_playlistMutex;
extern std::mutex g_errorMutex;
extern std::mutex g_ResolutionCacheMutex;

extern std::condition_variable g_serviceCv;
extern std::thread g_SidebarThread;

extern mpv_handle* mpv;

extern Uint32 off_ProcessEvents;
extern Uint32 lastUpdateTime;
extern  Uint32 invisibleStartTime;
extern Uint32 down_FPS_visible;
extern Uint32 frameDelay;
extern Uint32 frameStart;
extern Uint32 frameTime;
extern Uint32 mpvPollDelayWhenHidden ; 
extern Uint32 Relay_Ilde;
extern Uint32 g_lastSeekRequestTime ;
extern Uint32 lastInteractionTime;
extern Uint32 delayProcessEvents ;

extern bool is_live ;
extern bool Disabehotkey;
extern bool playImmediately;
extern bool file_local ;
extern bool showIOCHSettings;
extern bool loadingVideoformpopup ;
extern bool mpvUpdatePaused ;
extern bool reset_FPS_bool;
extern bool urlConfirmed;
extern bool needRender ;
extern bool render_video ;
extern bool render_ui_video ;
extern bool render_load;
extern bool g_WindowVisible ;
extern bool hasRenderedSomething ; 
//extern bool g_RequestToggleFullscreen;
extern bool g_isSeekPending ;
extern bool g_wasPlayingBeforeSeek;
extern bool g_isChangingVideo;
extern bool g_hasload ;
extern bool show_ui_video;
extern bool shouldSeekAfterResolutionChange ;
extern bool mpv_player_ready;
extern bool pausedFrameValid;
extern bool hasRenderedPausedFrame;
extern bool hasRenderedIdleFrame ;
extern bool autoScaleVideo;
extern bool useGlobalVideoSize;
extern bool fpsInited;
extern bool audio_Theme;
extern bool cookies;
extern bool tool_video;
extern std::wstring confirmed_url;
extern std::wstring Url;
extern std::wstring resultURL;

extern int selectedAudioIndex;
extern int g_CurrentIndex;
extern int WinW;
extern int WinH;
extern int WinX;
extern int WinY;
extern int reset_FPS_int;
extern int volume ;
extern int targetFPS ;
extern int relay_render_loading;
extern int pause_render_ui_video;
extern int pause_render_video;
extern int render_ilde_full;
extern int hiddenDelay ;       
extern int maxHiddenDelay ;
extern int currentIndex;


extern float g_seekTargetTime;
extern float g_seekingIconFade;
extern std::vector<std::wstring> playlist;

extern ReusableWindow g_urlWindow;
extern ReusablePopup Popup_Url;   
extern ReusablePopup videoInfoPopup;
extern ReusableWindow SidebarWindow;
extern ReusablePopup SettingPopup;
extern ReusablePopup SidarBarPopup;

extern std::queue<std::string> g_errorQueue;

extern double pendingSeekTime ;


extern GLuint pausedFrameTexture;

inline bool loadingTimeoutOccurred = false;

std::vector<ReusablePopup*>& GetAllPopups();

bool IsAnyPopupOpen();

void RenderAllPopups(struct mpv_handle* mpv);

void OffPopup();




