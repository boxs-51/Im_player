#ifndef PLAYER_CONTROLS_H
#define PLAYER_CONTROLS_H

#include <mpv/client.h>
#include <string>
#include <util.h>
#include <globals.h> 

// Tải và cache icon theo tên
GLuint GetIcon(const std::string& name);

// Vẽ giao diện điều khiển và xử lý tương tác
void RenderPlayerControls(mpv_handle* mpv, ImVec2& _pos , ImVec2& _size,
                          bool& isFullscreen_video,bool& show_ui_video);


void RenderIdleBackground(std::string& imagePath, ImVec2& _pos, ImVec2& _size) ;

void CleanupIcons();


void RenderLoading(ImVec2& _pos, ImVec2& _size);
void RenderSeekingOverlay(ImVec2& _pos , ImVec2& _size) ;
void RenderGhostStatusOverlay(ImVec2& vPos, ImVec2& vSize, bool isPaused);

void ShowTooltipDelayed(const char* text, bool hovering ,double delaySeconds = 0.5, const char* id = nullptr);


#endif // PLAYER_CONTROLS_H
