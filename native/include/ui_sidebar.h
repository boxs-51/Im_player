#ifndef UI_SIDEBAR_H
#define UI_SIDEBAR_H

#include <string>
#include <vector>
#include <mpv/client.h>

void RenderSidebarListUi(bool& requestClose, mpv_handle* mpv, std::vector<std::wstring>& playlist, int& currentIndex);

#endif
