#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <imgui.h>
#include "player/mpv_basic_formats.h"

class WindowRuntime;

void ResolutionQualityPage(WindowRuntime* runtime, VideoAudioFormats &formats, VideoType videotype, float scale);
void AudioQualityPage(WindowRuntime* runtime, VideoAudioFormats &formats, VideoType videotype, float scale);
void PlaybackSpeedPage(WindowRuntime* runtime, float scale);
//void OptionsPage();
void RenderIOCHSidebar(WindowRuntime* runtime, ImVec2 videoPos, ImVec2 videoSize, bool open, ImVec2 iconPos);
void ApplyPlaybackSettings(WindowRuntime* runtime);

#endif // UI_SETTINGS_H