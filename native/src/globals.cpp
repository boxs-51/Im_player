
// globals.cpp
#include "globals.h"

int main_loop_rate;
UiWindowsState uiState;

std::vector<std::string> g_keywords;

std::mutex g_mutex;
std::string g_nextPageToken;
std::string g_searchQuery;

bool Disabehotkey = false;
