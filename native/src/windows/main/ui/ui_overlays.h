#ifndef UI_OVERLAYS_H
#define UI_OVERLAYS_H

#include <imgui.h>
#include <string>

void RenderIdleBackground(const std::string& imagePath, const ImVec2& _pos, const ImVec2& _size);
void CleanupIcons();
void RenderLoading(const ImVec2& _pos, const ImVec2& _size);
void RenderSeekingOverlay(const ImVec2& _pos, const ImVec2& _size);
void RenderGhostStatusOverlay(const ImVec2& vPos, const ImVec2& vSize, bool isPaused);

#endif // UI_OVERLAYS_H