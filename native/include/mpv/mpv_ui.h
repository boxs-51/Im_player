#ifndef PLAYER_CONTROLS_H
#define PLAYER_CONTROLS_H

#include <mpv/client.h>

#include <string>

#include <globals.h> 
#include "imgui.h"  // Cần thiết vì dùng ImVec2

// Tải và cache icon theo tên
GLuint GetIcon(const std::string& name);

// Vẽ giao diện điều khiển và xử lý tương tác
void RenderPlayerControls(mpv_handle* mpv, ImVec2 videoPos , ImVec2 videoSize, SDL_Window* window,
                          bool& isFullscreen_video,bool& show_ui_video);


void RenderIdleBackground(ImTextureID texID, ImVec2 videopos, ImVec2 videoSize) ;

void CleanupIcons();


void RenderLoading( ImVec2 VideoPos, ImVec2 VideoSize);

void RenderSeekingOverlay( ImVec2 VideoPos , ImVec2 VideoSize ,SeekingData& data) ;


#endif // PLAYER_CONTROLS_H
