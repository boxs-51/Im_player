
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

SeekingData dataseek;
UiWindowsState uiState;
MPV mpv;

std::vector<std::string> g_keywords;

std::mutex g_mutex;
std::string g_nextPageToken;
std::string g_searchQuery;

bool Disabehotkey = false;
bool playImmediately = true;
bool Audio_visualizers = false;
bool was_ui_video = false;
double pendingSeekTime = -1.0;

Uint64 lastInteractionTime = 0;


std::vector<std::wstring> playlist;
