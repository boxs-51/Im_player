#ifndef UI_CONTROLS_H
#define UI_CONTROLS_H

#include <imgui.h>

class WindowRuntime;

void DrawTimeDisplay(double current, double duration, ImVec2& videoSize, ImVec2& pos, float parentHeight);
void RenderPlayerControls(WindowRuntime* runtime, const ImVec2& _pos, const ImVec2& _size);

#endif // UI_CONTROLS_H