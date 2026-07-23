
// globals.cpp
#include "globals.h"

int main_loop_rate;
UiWindowsState uiState;

std::vector<std::string> g_keywords;

std::mutex g_mutex;
std::string g_nextPageToken;
std::string g_searchQuery;

bool g_WindowVisible = false;
bool Disabehotkey = false;
bool playImmediately = true;
bool was_ui_video = false;
double pendingSeekTime = -1.0;

std::vector<std::wstring> playlist;
