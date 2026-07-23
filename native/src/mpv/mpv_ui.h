#ifndef PLAYER_CONTROLS_H
#define PLAYER_CONTROLS_H

#include <mpv/client.h>
#include <string>
#include <util.h>
#include <imgui.h>
#include <GL/gl3w.h> 
#include "WindowRuntime.h"
// Tải và cache icon theo tên
GLuint GetIcon(const std::string& name);

// Vẽ giao diện điều khiển và xử lý tương tác
void RenderPlayerControls(WindowRuntime* runtime, const ImVec2& _pos , const ImVec2& _size);


void RenderIdleBackground(const std::string& imagePath, const ImVec2& _pos, const ImVec2& _size) ;

void CleanupIcons();


void RenderLoading(const ImVec2& _pos, const ImVec2& _size);
void RenderSeekingOverlay(const ImVec2& _pos , const ImVec2& _size) ;
void RenderGhostStatusOverlay(const ImVec2& vPos, const ImVec2& vSize, bool isPaused);

void ShowTooltipDelayed(const char* text, bool hovering ,double delaySeconds = 0.5, const char* id = nullptr);


#endif // PLAYER_CONTROLS_H
