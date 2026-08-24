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


extern int main_loop_rate;
extern UiWindowsState uiState;

extern std::vector<std::string> g_keywords;

extern std::mutex g_mutex;
extern std::string g_nextPageToken;
extern std::string g_searchQuery;


extern bool Disabehotkey;






