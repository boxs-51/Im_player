
// globals.cpp
#include "globals.h"

UiWindowsState uiState;
MPV mpv;

std::vector<std::string> g_keywords;

std::mutex g_mutex;
std::string g_nextPageToken;
std::string g_searchQuery;

bool is_dirty = false;
bool g_WindowVisible = false;
bool Disabehotkey = false;
bool playImmediately = true;
bool Audio_visualizers = false;
bool was_ui_video = false;
double pendingSeekTime = -1.0;

Uint64 lastInteractionTime = 0;


std::vector<std::wstring> playlist;
