// globals.h
#pragma once

#include <SDL.h>
#include <string>
#include <vector>
#include <mutex>
#include <mpv/client.h>
#include <mpv/render.h>

struct UiWindowsState {
    bool show_demo = false;
    bool show_style = false;
    bool show_metrics = false;
    bool show_log = false;
    bool show_settings = false;
};

struct MPV
{
    mpv_handle* mpv; 
    mpv_render_context* render_ctx ;
};

extern MPV mpv;
extern UiWindowsState uiState;

extern std::vector<std::string> g_keywords;

extern std::mutex g_mutex;
extern std::string g_nextPageToken;
extern std::string g_searchQuery;

extern Uint64 lastInteractionTime;

extern bool Disabehotkey;
extern bool playImmediately;

extern bool is_dirty;
extern bool g_WindowVisible;
extern bool Audio_visualizers;
extern bool was_ui_video ;
extern std::vector<std::wstring> playlist;


extern double pendingSeekTime ;





