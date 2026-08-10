#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <functional>
#include <imgui.h>

void UI_MenuItem(const char* label, const char* current_value, float scale, std::function<void()> on_click);
void UI_Toggle(const char* label, bool* v, float scale, bool enabled, std::function<void(bool)> on_change);
void UI_GroupHeader(const char* title, float scale = 1.0f);
void UI_SliderSpeed(const char* label, float* value, float min, float max, float scale, std::function<void(float)> on_change);
void UI_SelectableItem(const char* label, bool is_active, float scale, std::function<void()> on_click);

#endif // UI_WIDGETS_H