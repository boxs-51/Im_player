#pragma once

#include "../core/gui_types.h"

using OldIconFn = void (*)(ImDrawList *, ImVec2, ImVec2, ImU32);

namespace CSImGui {
    const IconButtonStyle &GetDefaultIconButtonStyle();

    bool CustomIconButton(const char *str_id, void (*drawFn)(ImDrawList *, ImVec2, ImVec2, ImU32, void *), ImVec2 size, void *user_data, const IconButtonStyle &style);
    bool CustomIconButton(const char *str_id, void (*drawFn)(ImDrawList *, ImVec2, ImVec2, ImU32, void *), ImVec2 size);
    bool CustomIconButton(const char *str_id, OldIconFn oldFn, ImVec2 size);
    bool CustomIconButton(const char *str_id, void (*drawFn)(ImDrawList *, ImVec2, ImVec2, ImU32, void *), ImVec2 size, void *user_data);

    bool ModernButton(const char *label, const ImVec2 &size_arg = ImVec2(0, 0), bool primary = true);
    bool SecondaryButton(const char *label, const ImVec2 &size = ImVec2(0, 0));
    bool ModernButtonEx(const char *label, const ImVec2 &size_arg = ImVec2(0, 0));
    bool ModernSmallButton(const char *label);
    bool ModernArrowButton(const char *str_id, ImGuiDir dir, ImVec2 size = ImVec2(0, 0));
    bool ModernCheckbox(const char *label, bool *v, CheckboxStyle style = CheckboxStyle::Tick, const ImVec2 &size_arg = ImVec2(0, 0));
    bool ModernToggle(const char *str_id, bool *v, bool enabled = true, float scale = 1.0f);
}