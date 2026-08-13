#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include <functional>
#include <imgui.h>

#define SMOOTH_LERP(speed, dt) (1.0f - expf(-(speed) * (dt)))
#define IM_COL32_LERP(A, B, T)                                                                                                   \
    IM_COL32(                                                                                                                    \
        (ImU32)(((A) >> IM_COL32_R_SHIFT & 0xFF) + (((B) >> IM_COL32_R_SHIFT & 0xFF) - ((A) >> IM_COL32_R_SHIFT & 0xFF)) * (T)), \
        (ImU32)(((A) >> IM_COL32_G_SHIFT & 0xFF) + (((B) >> IM_COL32_G_SHIFT & 0xFF) - ((A) >> IM_COL32_G_SHIFT & 0xFF)) * (T)), \
        (ImU32)(((A) >> IM_COL32_B_SHIFT & 0xFF) + (((B) >> IM_COL32_B_SHIFT & 0xFF) - ((A) >> IM_COL32_B_SHIFT & 0xFF)) * (T)), \
        (ImU32)(((A) >> IM_COL32_A_SHIFT & 0xFF) + (((B) >> IM_COL32_A_SHIFT & 0xFF) - ((A) >> IM_COL32_A_SHIFT & 0xFF)) * (T)))

typedef uint8_t Col;
typedef uint8_t Var;
typedef uint16_t ModernTabFlags;
typedef uint16_t ModernWinDownFlags;
enum class Phase { None, AfterInit, Init, BeforeInie, Draw };
enum class Slot { None, Draw_layer0, Draw_layer1, Draw_layer2, Layout, End };
enum Var_ {
    Var_COUNT,
};

enum Col_ {
    Col_Text,
    Col_TextDisabled,
    Col_WindowBg,
    Col_ChildBg,
    Col_PopupBg,
    Col_Border,
    Col_BorderShadow,
    Col_FrameBg,
    Col_FrameBgHovered,
    Col_FrameBgActive,
    Col_TitleBg,
    Col_TitleBgActive,
    Col_TitleBgCollapsed,
    Col_MenuBarBg,
    Col_ScrollbarBg,
    Col_ScrollbarGrab,
    Col_ScrollbarGrabHovered,
    Col_ScrollbarGrabActive,
    Col_CheckMark,
    Col_SliderGrab,
    Col_SliderGrabHovered,
    Col_SliderGrabActive,
    Col_Button,
    Col_ButtonHovered,
    Col_ButtonActive,
    Col_Header,
    Col_HeaderHovered,
    Col_HeaderActive,
    Col_Separator,
    Col_SeparatorHovered,
    Col_SeparatorActive,
    Col_ResizeGrip,
    Col_ResizeGripHovered,
    Col_ResizeGripActive,
    Col_InputTextCursor,
    Col_TabHovered,
    Col_Tab,
    Col_TabSelected,
    Col_TabSelectedOverline,
    Col_TabActive,
    Col_TabUnfocused,
    Col_TabUnfocusedActive,
    Col_TabDimmed,
    Col_TabDimmedSelected,
    Col_TabDimmedSelectedOverline,
    Col_DockingPreview,
    Col_DockingEmptyBg,
    Col_PlotLines,
    Col_PlotLinesHovered,
    Col_PlotHistogramHovered,
    Col_PlotHistogramActive,
    Col_PlotHistogram,
    Col_TableHeaderBg,
    Col_TableBorderStrong,
    Col_TableBorderLight,
    Col_TableRowBg,
    Col_TableRowBgAlt,
    Col_TextLink,
    Col_TextSelected,
    Col_TextSelectedBg,
    Col_TreeLines,
    Col_DragDropTarget,
    Col_NavCursor,
    Col_NavWindowingHighlight,
    Col_NavWindowingDimBg,
    Col_ModalWindowDimBg,
    Col_COUNT
};

enum ModernTabFlags_ {
    ModernTabFlags_None = 0,
    ModernTabFlags_NoIndicator = 1 << 0,
    ModernTabFlags_NoAnimation = 1 << 1,
    ModernTabFlags_FullWidthBar = 1 << 2,
};

enum ModernWinDownFlags_ {
    ModernWinDownFlags_None = 0,
    ModernWinDownFlags_NoTitleBar = 1 << 0,
};

enum class CheckboxStyle : uint8_t {
    Circle,
    Tick,
    Square
};

enum class ThemeType : uint8_t {
    DarkMode,
    LightMode,
    MidnightMode,
    RetroMode
};

struct Style {
    ImVec4 Colors[Col_COUNT];
};

struct ThemeTransition {
    Style startTheme;
    Style targetTheme;
    float progress = 1.0f;
    float speed = 2.5f;
    bool active = false;
};

enum class TextEffect {
    None, Gradient, Outline, Wave, Typewriter, Shimmer, FadeIn, Scrolling
};

struct TextEffectStyle {
    TextEffect effect = TextEffect::None;
    ImU32 colorTopLeft = IM_COL32(255, 255, 255, 255);
    ImU32 colorBottomRight = IM_COL32(0, 180, 255, 255);
    ImU32 outlineColor = IM_COL32(0, 0, 0, 200);
    float outlineThickness = 1.0f;
    float waveAmplitude = 3.0f;
    float waveFrequency = 4.0f;
    float waveSpeed = 3.0f;
    float progress = 1.0f;
    float speed = 1.0f;
    float fontScale = 1.0f;
    float scrollWidth = 200.0f;
    float scrollSpeed = 40.0f;
    float scrollGap = 30.0f;
    bool scrollOnlyOnHover = false;
};
struct TableCol {
    const char *name;
    float width;
};


struct CardHoleStyle {
    float rounding = 6.0f;
    float borderThickness = 1.5f;
};

struct IconButtonStyle {
    bool drawButtonBg = true;
    ImU32 buttonBgColor = IM_COL32(255, 255, 255, 20);
    ImU32 buttonBgHovered = IM_COL32(255, 255, 255, 45);
    ImU32 buttonBgActive = IM_COL32(255, 255, 255, 65);
    float buttonRounding = 8.0f;
    bool drawButtonBorder = true;
    ImU32 buttonBorderColor = IM_COL32(255, 255, 255, 30);
    float buttonBorderThickness = 1.0f;
    bool drawIconBg = false;
    ImU32 iconBgColor = IM_COL32(0, 0, 0, 40);
    float iconBgRounding = 4.0f;
    bool drawIconBorder = false;
    ImU32 iconBorderColor = IM_COL32(255, 255, 255, 160);
    float iconBorderThickness = 1.2f;
    ImU32 iconNormal = IM_COL32(230, 230, 230, 200);
    ImU32 iconHovered = IM_COL32(255, 255, 255, 255);
    ImU32 iconActive = IM_COL32(180, 210, 255, 255);
    float iconHoverScale = 1.05f;
    float iconActiveScale = 0.95f;
};
