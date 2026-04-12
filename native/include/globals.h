// globals.h
#pragma once
#include "reusable_popup.h"
#include "utils.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "mpv_settings.h"
#include "mpv_controller.h" 
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
#include <thread>
#include <mutex>
#include <algorithm>
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <condition_variable>
#include <mpv/render_gl.h>

struct UiWindowsState {
    bool show_demo = false;
    bool show_style = false;
    bool show_metrics = false;
    bool show_log = false;
    bool show_settings = false;
};
struct SeekingData {
    float timer = 0.0f;
    float alpha = 0.0f;   
    float pulse = 0.0f;   
    bool  forward = true;

    ImVec2 pos = ImVec2(0, 0);  
    ImVec2 size = ImVec2(0, 0);
    
    bool g_isSeeking =false;
};

struct MPV
{
    mpv_handle* mpv; 
    mpv_render_context* render_ctx ;
};

extern MPV mpv;
extern SeekingData dataseek;
extern UiWindowsState uiState;

extern std::vector<std::string> g_keywords;

extern std::mutex g_mutex;
extern std::string g_nextPageToken;
extern std::string g_searchQuery;

extern Uint64 lastInteractionTime;

extern bool ToggleFullscreen ;
extern bool Disabehotkey;
extern bool playImmediately;

extern bool Audio_visualizers;

extern std::vector<std::wstring> playlist;

extern std::queue<std::string> g_errorQueue;

extern double pendingSeekTime ;





