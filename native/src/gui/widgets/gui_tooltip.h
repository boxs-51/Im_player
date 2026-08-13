#pragma once

#include "../core/gui_types.h"
#include <string>
#include <vector>

namespace ImGuiTooltip {
    enum Alignment { Align_Left, Align_Center, Align_Right };
    enum LayoutMode { Layout_Vertical, Layout_Horizontal, Layout_Custom };
    enum TooltipDirection { Dir_Up, Dir_Down, Dir_Left, Dir_Right, Dir_UpLeft, Dir_UpRight, Dir_DownLeft, Dir_DownRight };
}

struct TooltipItemData {
    bool is_visible = false;
    bool active = false;
    bool hovered = false;
    float hovered_time = 0.0f;
    float hover_delay = 0.2f;
    float value = 0.0f;
    float seek_value = 0.0f;
    float v_min = 0.0f;
    float range = 0.0f;
    ImVec2 center = ImVec2(0, 0);
    ImVec2 pos = ImVec2(0, 0);
    ImVec2 size = ImVec2(0, 0);
    ImVec2 mouse = ImVec2(0, 0);
    ImVec2 cursor_size = ImVec2(0, 0);
};

struct TooltipBeginData {
    ImGuiID id = 0;
    ImFont *font = nullptr;
    ImDrawList *draw_list = nullptr;
    float dt = 8.0f;
    float fontsize = 12.0f;
    std::string text;
    std::string title;
    std::string extra;
    ImVec2 title_size;
    ImVec2 extra_size;
    ImVec2 text_size;
    bool show_title = false;
    bool show_extra = false;
    bool show_text = true;
    int title_max_lines = 2;
    const char *format = "%.03f";
    ImGuiTooltip::Alignment align = ImGuiTooltip::Align_Center;

    std::vector<std::string> cached_title;
    std::vector<std::string> cached_text;
    std::vector<std::string> cached_extra;
    std::string last_raw_title;
    std::string last_raw_text;
    std::string last_raw_extra;

    ImTextureID image = 0;
    ImVec2 image_size = ImVec2(0, 0);
    bool show_image = false;
    float aspect_ratio = 16.0f / 9.0f;
    bool lock_aspect = true;

    ImGuiTooltip::LayoutMode layout = ImGuiTooltip::Layout_Vertical;
    ImGuiTooltip::TooltipDirection dir = ImGuiTooltip::Dir_Down;
    bool lock_dir = false;

    float padding_content = 6.0f;
    float spacing_content = 4.0f;
    float padding = 0.0f;
    float spacing_mouse = 12.0f;
    float rounding = 4.0f;
    float border_thickness = 1.0f;
    float edge_safe_padding = 1.0f;

    float arrow_width = 5.0f;
    float arrow_height = 5.0f;
    float arrow_scale_min = 0.5f;
    float arrow_scale_max = 1.2f;
    float arrow_corner_bias = 0.25f;

    float max_width = 260.0f;
    float max_height = 200.0f;

    ImU32 col_bg = IM_COL32(0, 0, 0, 200);
    ImU32 col_text = IM_COL32(255, 255, 255, 255);
    ImU32 col_border = IM_COL32(255, 255, 255, 255);
    ImU32 col_title = IM_COL32(255, 255, 120, 255);
    ImU32 col_extra = IM_COL32(180, 180, 180, 255);

    bool skip_draw_bg = false;
    bool skip_draw_border = false;
    bool skip_draw_text = false;
    bool skip_draw_arrow = false;
    bool skip_draw = false;
};

struct TooltipAnimState {
    float alpha = 0.0f;
    float speedfade = 12.0f;
    float speedease = 15.0f;
    ImVec2 pos = ImVec2(0, 0);
    ImVec2 size = ImVec2(0, 0);
    ImVec2 last_arrow_dir = ImVec2(0, -1);
};

struct TooltipData {
    TooltipItemData item;
    TooltipBeginData config;
    TooltipAnimState *anim = nullptr;
    ImVec2 out_pos = ImVec2(0, 0);
    ImVec2 out_size = ImVec2(0, 0);
};

typedef uint16_t ToolTipFlags;
enum ToolTipFlags_ {
    ToolTipFlags_None = 0,
    ToolTipFlags_FollowMouse = 1 << 0,
    ToolTipFlags_Ease = 1 << 1,
    ToolTipFlags_Fade = 1 << 2,
    ToolTipFlags_FollowMouse_Fixed_Y = 1 << 3,
    ToolTipFlags_FollowMouse_Fixed_X = 1 << 4,
    ToolTipFlags_Fixed = 1 << 5,
    ToolTipFlags_Hiden = 1 << 6,
    ToolTipFlags_AlwaysShow = 1 << 7,
    ToolTipFlags_AlwaysShowAction = 1 << 8,
    ToolTipFlags_Scale = 1 << 9,
    ToolTipFlags_NoBorder = 1 << 10,
    ToolTipFlags_NoArrow = 1 << 11,
    ToolTipFlags_ClampItem = 1 << 12,
    ToolTipFlags_ClampWindow = 1 << 13,
    ToolTipFlags_AutoPosition = 1 << 14,
    ToolTipFlags_Animation = ToolTipFlags_Ease | ToolTipFlags_Fade | ToolTipFlags_Scale
};

using TooltipCallback = std::function<void(enum class Phase phase, enum class Slot slot, TooltipData *data, ImDrawList *draw_list)>;

void ToolTipEx(TooltipData *td, ToolTipFlags flags = ToolTipFlags_None, TooltipCallback tooltip_cb = nullptr);

namespace CSImGui {
    void ShowTooltipDelayed(const char *text, bool hovering, double delaySeconds);
    bool ToolTip(const char *label, float delay = 3.0f, ToolTipFlags flags = ToolTipFlags_None);
}