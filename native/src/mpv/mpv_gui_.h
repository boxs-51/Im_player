
#pragma once
#include <map>
#include <utils.h>
#include <imgui.h>



enum Col_
{
    Col_Text,
    Col_TextDisabled,
    Col_WindowBg,              // Background of normal windows
    Col_ChildBg,               // Background of child windows
    Col_PopupBg,               // Background of popups, menus, tooltips windows
    Col_Border,
    Col_BorderShadow,
    Col_FrameBg,               // Background of checkbox, radio button, plot, slider, text input
    Col_FrameBgHovered,
    Col_FrameBgActive,
    Col_TitleBg,               // Title bar
    Col_TitleBgActive,         // Title bar when focused
    Col_TitleBgCollapsed,      // Title bar when collapsed
    Col_MenuBarBg,
    Col_ScrollbarBg,
    Col_ScrollbarGrab,
    Col_ScrollbarGrabHovered,
    Col_ScrollbarGrabActive,
    Col_CheckMark,             // Checkbox tick and RadioButton circle
    Col_SliderGrab,
    Col_SliderGrabHovered,
    Col_SliderGrabActive,
    Col_Button,
    Col_ButtonHovered,
    Col_ButtonActive,
    Col_Header,                // Header* color are used for CollapsingHeader, TreeNode, Selectable, MenuItem
    Col_HeaderHovered,
    Col_HeaderActive,
    Col_Separator,
    Col_SeparatorHovered,
    Col_SeparatorActive,
    Col_ResizeGrip,            // Resize grip in lower-right and lower-left corners of windows.
    Col_ResizeGripHovered,
    Col_ResizeGripActive,
    Col_InputTextCursor,       // InputText cursor/caret
    Col_TabHovered,            // Tab background, when hovered
    Col_Tab,                   // Tab background, when tab-bar is focused & tab is unselected
    Col_TabSelected,           // Tab background, when tab-bar is focused & tab is selected
    Col_TabSelectedOverline,   // Tab horizontal overline, when tab-bar is focused & tab is selected
    Col_TabActive,
    Col_TabUnfocused,
    Col_TabUnfocusedActive,
    Col_TabDimmed,             // Tab background, when tab-bar is unfocused & tab is unselected
    Col_TabDimmedSelected,     // Tab background, when tab-bar is unfocused & tab is selected
    Col_TabDimmedSelectedOverline,//..horizontal overline, when tab-bar is unfocused & tab is selected
    Col_DockingPreview,        // Preview overlay color when about to docking something
    Col_DockingEmptyBg,        // Background color for empty node (e.g. CentralNode with no window docked into it)
    Col_PlotLines,
    Col_PlotLinesHovered,
    Col_PlotHistogramHovered,
    Col_PlotHistogramActive,
    Col_PlotHistogram,
    Col_TableHeaderBg,         // Table header background
    Col_TableBorderStrong,     // Table outer and header borders (prefer using Alpha=1.0 here)
    Col_TableBorderLight,      // Table inner borders (prefer using Alpha=1.0 here)
    Col_TableRowBg,            // Table row background (even rows)
    Col_TableRowBgAlt,         // Table row background (odd rows)
    Col_TextLink,              // Hyperlink color
    Col_TextSelected,
    Col_TextSelectedBg,        // Selected text inside an InputText
    Col_TreeLines,             // Tree node hierarchy outlines when using ImGuiTreeNodeFlags_DrawLines
    Col_DragDropTarget,        // Rectangle highlighting a drop target
    Col_NavCursor,             // Color of keyboard/gamepad navigation cursor/rectangle, when visible
    ol_NavWindowingHighlight, // Highlight window when using CTRL+TAB
    Col_NavWindowingDimBg,     // Darken/colorize entire screen behind the CTRL+TAB window list, when active
    Col_ModalWindowDimBg,      // Darken/colorize entire screen behind a modal window, when one is active
    Col_COUNT
};
enum ModernTabFlags_ {
    ModernTabFlags_None             = 0,
    ModernTabFlags_NoIndicator      = 1 << 0, // Tắt thanh kẻ dưới chân
    ModernTabFlags_NoAnimation      = 1 << 1, // Tắt hiệu ứng mượt
    ModernTabFlags_FullWidthBar     = 1 << 2, // Thanh kẻ dài 100% thay vì 80%
};
typedef int ModernTabFlags;
enum class ThemeType {
    DarkMode,
    LightMode,
    MidnightMode,
    RetroMode
};

struct Stytle{
    ImVec4 Colors[Col_COUNT];
};
inline Stytle dhs;
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline ImVec2 Lerp(const ImVec2& a, const ImVec2& b, float t)
{
    return ImVec2(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    );
}

inline ImVec4 Lerp(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}
inline void SetDarkTheme(Stytle& dhs) {
    auto& color = dhs.Colors;

    color[Col_Text]              = ImVec4(1.00f, 1.00f, 1.00f, 0.95f);
    color[Col_TextDisabled]      = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    color[Col_WindowBg]          = ImVec4(0.10f, 0.10f, 0.12f, 0.95f);
    color[Col_ChildBg]           = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    color[Col_FrameBg]           = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    color[Col_FrameBgHovered]    = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
    color[Col_FrameBgActive]     = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    color[Col_PopupBg]           = ImVec4(0.08f, 0.08f, 0.09f, 0.98f);
    
    color[Col_Border]            = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    color[Col_Separator]         = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);


    color[Col_TextSelected]      = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    color[Col_TextSelectedBg]    = ImVec4(0.12f, 0.45f, 0.90f, 0.35f);
    color[Col_TitleBg]           = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    color[Col_TitleBgActive]     = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);

    // --- Tương tác (Buttons / Checkbox) ---
    color[Col_Button]           = ImVec4(0.12f, 0.45f, 0.90f, 1.00f);
    color[Col_ButtonHovered]     = ImVec4(0.15f, 0.55f, 1.00f, 1.00f);
    color[Col_ButtonActive]    = ImVec4(0.10f, 0.35f, 0.80f, 1.00f);
    color[Col_CheckMark] = ImVec4(0.12f, 0.45f, 0.90f, 1.00f);

    // --- Headers & Tabs ---
    color[Col_Header] = ImVec4(0.18f, 0.21f, 0.25f, 1.00f);
    color[Col_HeaderHovered] = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    color[Col_HeaderActive] = ImVec4(0.28f, 0.33f, 0.40f, 1.00f);
    color[Col_Tab] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    color[Col_TabHovered] = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
    color[Col_TabActive] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    color[Col_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    color[Col_TabUnfocusedActive]  = ImVec4(0.12f, 0.45f, 0.90f, 0.40f);

    // --- Tables ---
    color[Col_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    color[Col_TableHeaderBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);

    color[Col_PlotHistogram] = ImVec4(0.18f, 0.50f, 0.92f, 1.0f);
    color[Col_PlotHistogramActive] = ImVec4(0.20f, 0.55f, 0.90f, 1.0f);
    color[Col_SliderGrab] = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    color[Col_SliderGrabHovered] = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
    color[Col_SliderGrabActive] = ImVec4(0.15f, 0.45f, 0.75f, 1.0f);
}
inline void SetLightTheme(Stytle& dhs) {
    auto& color = dhs.Colors;

    color[Col_WindowBg] = ImVec4(0.94f, 0.94f, 0.96f, 1.00f);
    color[Col_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    color[Col_FrameBgHovered] = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    color[Col_FrameBgActive] = ImVec4(0.90f, 0.90f, 0.93f, 1.00f);
    color[Col_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    color[Col_ChildBg] = ImVec4(0.97f, 0.97f, 0.98f, 1.00f);
    color[Col_Border] = ImVec4(0.75f, 0.75f, 0.80f, 1.00f);
    color[Col_Separator] = ImVec4(0.75f, 0.75f, 0.78f, 1.00f);

    color[Col_Text] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    color[Col_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    color[Col_TextSelected] = ImVec4(0.00f, 0.10f, 0.25f, 1.00f);
    color[Col_TextSelectedBg] = ImVec4(0.00f, 0.47f, 0.83f, 0.25f);
    color[Col_TitleBg] = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    color[Col_TitleBgActive] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);

    color[Col_Button] = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);
    color[Col_ButtonHovered] = ImVec4(0.05f, 0.55f, 0.95f, 1.00f);
    color[Col_ButtonActive] = ImVec4(0.00f, 0.40f, 0.75f, 1.00f);
    color[Col_CheckMark] = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);

    color[Col_Header] = ImVec4(0.90f, 0.92f, 0.96f, 1.00f);
    color[Col_HeaderHovered] = ImVec4(0.85f, 0.88f, 0.94f, 1.00f);
    color[Col_HeaderActive]= ImVec4(0.80f, 0.84f, 0.90f, 1.00f);
    color[Col_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TabHovered]= ImVec4(0.80f, 0.80f, 0.85f, 1.00f);
    color[Col_TabActive]= ImVec4(0.70f, 0.70f, 0.75f, 1.00f);
    color[Col_TabUnfocused]= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TabUnfocusedActive] = ImVec4(0.00f, 0.47f, 0.83f, 0.40f);

    color[Col_TableRowBg]= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TableRowBgAlt]= ImVec4(0.00f, 0.00f, 0.00f, 0.03f);
    color[Col_TableHeaderBg]= ImVec4(0.85f, 0.85f, 0.88f, 1.00f);

    color[Col_PlotHistogram]= ImVec4(0.00f, 0.45f, 0.80f, 1.0f);
    color[Col_PlotHistogramActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    color[Col_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    color[Col_SliderGrabHovered]   = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
    color[Col_SliderGrabActive]   = ImVec4(0.00f, 0.47f, 0.83f, 1.0f);
}
inline void SetMidnightTheme(Stytle& dhs) {
    auto& color = dhs.Colors;

    color[Col_WindowBg]= ImVec4(0.07f, 0.08f, 0.11f, 0.98f);
    color[Col_FrameBg]= ImVec4(0.09f, 0.11f, 0.16f, 1.00f);
    color[Col_FrameBgHovered]= ImVec4(0.14f, 0.17f, 0.25f, 1.00f);
    color[Col_FrameBgActive]= ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
    color[Col_PopupBg]= ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    color[Col_ChildBg]= ImVec4(0.09f, 0.11f, 0.16f, 1.00f);
    color[Col_Border]= ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    color[Col_Separator]= ImVec4(0.18f, 0.22f, 0.30f, 1.00f);

    color[Col_Text]= ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
    color[Col_TextDisabled]= ImVec4(0.40f, 0.45f, 0.55f, 1.00f);
    color[Col_TextSelected]= ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    color[Col_TextSelectedBg]= ImVec4(0.25f, 0.45f, 0.90f, 0.45f);
    color[Col_TitleBg]= ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    color[Col_TitleBgActive]= ImVec4(0.09f, 0.11f, 0.15f, 1.00f);

    color[Col_Button]= ImVec4(0.20f, 0.40f, 0.75f, 1.00f);
    color[Col_ButtonHovered]= ImVec4(0.25f, 0.50f, 0.90f, 1.00f);
    color[Col_ButtonActive]= ImVec4(0.15f, 0.35f, 0.65f, 1.00f);
    color[Col_CheckMark]= ImVec4(0.30f, 0.55f, 1.00f, 1.00f);

    color[Col_Header]= ImVec4(0.18f, 0.25f, 0.40f, 1.00f);
    color[Col_HeaderHovered]= ImVec4(0.22f, 0.30f, 0.50f, 1.00f);
    color[Col_HeaderActive]= ImVec4(0.28f, 0.38f, 0.60f, 1.00f);
    color[Col_Tab]= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TabHovered]= ImVec4(0.20f, 0.25f, 0.40f, 1.00f);
    color[Col_TabActive]= ImVec4(0.15f, 0.18f, 0.30f, 1.00f);
    color[Col_TabUnfocused]= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TabUnfocusedActive] = ImVec4(0.25f, 0.45f, 0.90f, 0.60f);

    color[Col_TableRowBg]= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TableRowBgAlt]= ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    color[Col_TableHeaderBg]= ImVec4(0.12f, 0.15f, 0.22f, 1.00f);

    color[Col_PlotHistogram]= ImVec4(0.70f, 0.20f, 0.90f, 1.0f);
    color[Col_PlotHistogramActive] = ImVec4(0.60f, 0.35f, 0.90f, 1.0f);
    color[Col_SliderGrab]   = ImVec4(0.10f, 0.90f, 0.60f, 1.0f);
    color[Col_SliderGrabHovered]   = ImVec4(0.35f, 0.35f, 0.60f, 1.0f);
    color[Col_SliderGrabActive]   = ImVec4(0.00f, 1.00f, 0.80f, 1.0f);
}
inline void SetRetroTheme(Stytle& dhs) {
    auto& color = dhs.Colors;

    color[Col_WindowBg]= ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    color[Col_FrameBg]= ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    color[Col_FrameBgHovered]= ImVec4(0.20f, 0.19f, 0.18f, 1.00f);
    color[Col_FrameBgActive]= ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    color[Col_PopupBg]= ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    color[Col_ChildBg]= ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    color[Col_Border]= ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    color[Col_Separator]= ImVec4(0.31f, 0.29f, 0.27f, 1.00f);

    color[Col_Text]= ImVec4(0.92f, 0.86f, 0.70f, 1.00f);
    color[Col_TextDisabled]= ImVec4(0.50f, 0.45f, 0.40f, 1.00f);
    color[Col_TextSelected]= ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    color[Col_TextSelectedBg]= ImVec4(0.84f, 0.60f, 0.13f, 0.35f);
    color[Col_TitleBg]= ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    color[Col_TitleBgActive]= ImVec4(0.15f, 0.14f, 0.13f, 1.00f);

    color[Col_Button]= ImVec4(0.84f, 0.60f, 0.13f, 1.00f);
    color[Col_ButtonHovered]= ImVec4(0.98f, 0.74f, 0.18f, 1.00f);
    color[Col_ButtonActive]= ImVec4(0.72f, 0.51f, 0.10f, 1.00f);
    color[Col_CheckMark]= ImVec4(0.58f, 0.63f, 0.13f, 1.00f);

    color[Col_Header]= ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    color[Col_HeaderHovered]= ImVec4(0.40f, 0.36f, 0.33f, 1.00f);
    color[Col_HeaderActive]= ImVec4(0.25f, 0.24f, 0.23f, 1.00f);
    color[Col_Tab]= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TabHovered]= ImVec4(0.30f, 0.28f, 0.26f, 1.00f);
    color[Col_TabActive]= ImVec4(0.25f, 0.23f, 0.21f, 1.00f);
    color[Col_TabUnfocused]= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TabUnfocusedActive] = ImVec4(0.84f, 0.60f, 0.13f, 0.50f);

    color[Col_TableRowBg]= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    color[Col_TableRowBgAlt]= ImVec4(1.00f, 0.90f, 0.70f, 0.02f);
    color[Col_TableHeaderBg ]= ImVec4(0.18f, 0.17f, 0.16f, 1.00f);

    color[Col_PlotHistogram] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
    color[Col_PlotHistogramActive] = ImVec4(0.00f, 0.00f, 0.50f, 1.0f);
    color[Col_SliderGrab]   = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    color[Col_SliderGrabHovered]   = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    color[Col_SliderGrabActive]   = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
}
 
struct ThemeTransition {
    Stytle startTheme;   // Màu lúc bắt đầu bấm nút
    Stytle targetTheme;  // Màu đích muốn tới
    float progress = 1.0f;    // 1.0 nghĩa là đã xong, < 1.0 là đang chạy
    float speed = 2.5f;       // Tốc độ chuyển đổi
    bool active = false;
};

inline ThemeTransition gtr;
inline std::map<ThemeType, Stytle> ThemeLibrary;
inline void ApplyTheme(ThemeType type ) {
    if (ThemeLibrary.find(type) == ThemeLibrary.end()) return;

    gtr.startTheme = dhs;              // Lưu trạng thái hiện tại làm điểm gốc
    gtr.targetTheme = ThemeLibrary[type]; // Lấy theme đích từ thư viện
    gtr.progress = 0.0f;                  // Reset tiến trình về 0
    gtr.active = true;
}
inline void InitThemeLibrary(ThemeType type = ThemeType::DarkMode ) {
    // Theme Dark
    SetDarkTheme(dhs); // Hàm cũ của bạn
    ThemeLibrary[ThemeType::DarkMode] = dhs;

    // Theme Light
    SetLightTheme(dhs); // Hàm cũ của bạn
    ThemeLibrary[ThemeType::LightMode] = dhs;

    SetMidnightTheme(dhs);
    ThemeLibrary[ThemeType::MidnightMode] = dhs;

    SetRetroTheme(dhs);
    ThemeLibrary[ThemeType::RetroMode] = dhs;

    ApplyTheme(type);
}
inline void UpdateTheme(float deltaTime) {
    if (!gtr.active) return;
    gtr.progress += deltaTime * gtr.speed;
    float t = (gtr.progress > 1.0f) ? 1.0f : gtr.progress;

    // Ép kiểu sang float* để nội suy toàn bộ struct Style (bao gồm mảng Colors và các biến float lẻ)
    float* current = (float*)&dhs;
    float* start = (float*)&gtr.startTheme;
    float* target = (float*)&gtr.targetTheme;

    size_t numFloats = sizeof(Stytle) / sizeof(float);

    for (size_t i = 0; i < numFloats; i++) {
        current[i] = Lerp(start[i], target[i], t);
    }

    if (gtr.progress >= 1.0f) gtr.active = false;
}
inline ImVec4 GetColors(Col_ idx) {
    return dhs.Colors[idx];
}
