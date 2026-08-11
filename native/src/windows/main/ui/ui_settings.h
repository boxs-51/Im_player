#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <imgui.h>

class WindowRuntime;

void PlaybackSpeedPage(WindowRuntime* runtime, float scale);
//void OptionsPage();
void RenderIOCHSidebar(WindowRuntime* runtime, ImVec2 videoPos, ImVec2 videoSize, bool open, ImVec2 iconPos);

#endif // UI_SETTINGS_H