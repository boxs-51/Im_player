#include <gui/gui.h>
#include <gui/gui_widgets.h>
#include <map>


void SetDarkTheme(Stytle& dhs) {
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
void SetLightTheme(Stytle& dhs) {
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
void SetMidnightTheme(Stytle& dhs) {
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
void SetRetroTheme(Stytle& dhs) {
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

static std::map<ThemeType,Stytle> ThemeLibrary;
static ThemeTransition gtr;
static Stytle dhs; // current style đang dùng, sẽ được nội suy dần dần khi chuyển theme

void CSImGui::ApplyTheme(ThemeType themetype ) {
    if (ThemeLibrary.find(themetype) == ThemeLibrary.end()) return;

    gtr.startTheme = dhs;              // Lưu trạng thái hiện tại làm điểm gốc
    gtr.targetTheme = ThemeLibrary[themetype]; // Lấy theme đích từ thư viện
    gtr.progress = 0.0f;                  // Reset tiến trình về 0
    gtr.active = true;
}
void CSImGui::InitThemeLibrary(ThemeType themetype) {
    // Theme Dark
    Stytle tmp;
    SetDarkTheme(tmp); // Hàm cũ của bạn
    ThemeLibrary[ThemeType::DarkMode] = tmp;

    // Theme Light
    SetLightTheme(tmp); // Hàm cũ của bạn
    ThemeLibrary[ThemeType::LightMode] = tmp;

    SetMidnightTheme(tmp);
    ThemeLibrary[ThemeType::MidnightMode] = tmp;

    SetRetroTheme(tmp);
    ThemeLibrary[ThemeType::RetroMode] = tmp;

    CSImGui::ApplyTheme(themetype);
}
void CSImGui::UpdateTheme(float deltaTime) {
    if (!gtr.active) return;
    gtr.progress += deltaTime * gtr.speed;
    float t = (gtr.progress > 1.0f) ? 1.0f : gtr.progress;

    // Ép kiểu sang float* để nội suy toàn bộ struct Style (bao gồm mảng Colors và các biến float lẻ)
    float* current = (float*)&dhs;
    float* start = (float*)&gtr.startTheme;
    float* target = (float*)&gtr.targetTheme;

    size_t numFloats = sizeof(::Stytle) / sizeof(float);

    for (size_t i = 0; i < numFloats; i++) {
        current[i] = ImLerp(start[i], target[i], t);
    }

    if (gtr.progress >= 1.0f) gtr.active = false;
}

ImVec4& CSImGui::GetColors(::Col idx) {
    return dhs.Colors[idx];
}
Stytle& CSImGui::GetStyle(){
    return dhs;
}