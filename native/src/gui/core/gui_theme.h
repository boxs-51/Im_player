#pragma once

#include "gui_types.h"

namespace CSImGui {
    void ApplyTheme(ThemeType themetype);
    void InitThemeLibrary(ThemeType themetype);
    void UpdateTheme(float deltaTime);
    ImVec4 &GetColors(Col idx);
    Style &GetStyle();
}