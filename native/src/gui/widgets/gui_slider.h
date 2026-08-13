#pragma once

#include "../core/gui_types.h"
#include "gui_tooltip.h"

struct SliderRenderData {
    ImVec2 track_p1 = ImVec2(0, 0);
    ImVec2 track_p2 = ImVec2(0, 0);
    ImVec2 fill_p2 = ImVec2(0, 0);
    ImVec2 grab_center = ImVec2(0, 0);

    float grab_radius = 5.0f;
    float track_height = 5.0f;
    float buffer_t = -1.0f;
    float chapter_range_start = -1.0f;
    float chapter_range_end = -1.0f;
    float height = 0.0f;
    float height_on_hover = 0.0f;
    float marker_thickness = 2.0f;
    float grab_shadow_size = 3.0f;

    float value = 0.0f;
    float t = 0.0f;
    float visual_t = 0.0f;

    bool active = false;
    bool hovered = false;
    bool bar_hovered = false;
    bool grab_hovered = false;

    enum SlideDir { None, Left, Right, Up, Down } slide_dir = None;

    ImVec4 col_track = ImVec4(0.235f, 0.235f, 0.235f, 0.706f);
    ImVec4 col_fill = ImVec4(0.392f, 0.392f, 0.392f, 1.000f);
    ImVec4 col_border = ImVec4(0.392f, 0.392f, 0.392f, 1.000f);
    ImVec4 col_grab = ImVec4(0.588f, 0.588f, 0.588f, 1.000f);
    ImVec4 col_grab_border = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
    ImVec4 col_buffer = ImVec4(0.392f, 0.392f, 0.392f, 0.502f);
    ImVec4 col_marker = ImVec4(1.000f, 1.000f, 1.000f, 0.502f);
    ImVec4 col_chapter_range = ImVec4(0.392f, 0.392f, 1.000f, 0.235f);
    ImVec4 col_grab_shadow = ImVec4(0.000f, 0.000f, 0.000f, 0.117f);

    bool skip_draw_track = false;
    bool skip_draw_fill = false;
    bool skip_draw_grab = false;
    bool skip_draw_border = false;
    bool skip_draw_buffer = false;
    bool skip_draw_markers = false;
    bool skip_draw_chapter_range = false;
    bool skip_draw = false;

    std::vector<float> markers = {};

    struct SliderAnimState {
        float hover = 0.0f;
        float active = 0.0f;
        float bar_hover = 0.0f;
        float grab_hover = 0.0f;
        float grab_scale = 1.0f;
        float speedease = 15.0f;
        float bar_height_scale = 1.0f;

        ImVec4 col_track = ImVec4(0.235f, 0.235f, 0.235f, 0.706f);
        ImVec4 col_fill = ImVec4(0.392f, 0.392f, 0.392f, 1.000f);
        ImVec4 col_border = ImVec4(0.392f, 0.392f, 0.392f, 1.000f);
        ImVec4 col_grab = ImVec4(0.588f, 0.588f, 0.588f, 1.000f);
        ImVec4 col_grab_border = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
        ImVec4 col_buffer = ImVec4(0.392f, 0.392f, 0.392f, 0.502f);
        ImVec4 col_marker = ImVec4(1.000f, 1.000f, 1.000f, 0.502f);
        ImVec4 col_chapter_range = ImVec4(0.392f, 0.392f, 1.000f, 0.235f);
        ImVec4 col_grab_shadow = ImVec4(0.000f, 0.000f, 0.000f, 0.117f);
    } *anim = nullptr;
};

typedef uint16_t SliderFlags;
enum SliderFlags_ {
    SliderFlags_None = 0,
    SliderFlags_TooltipHiden = 1 << 0,
    SliderFlags_TooltipAlwaysShow = 1 << 1,
    SliderFlags_TooltipAlwaysShowAction = 1 << 2,
    SliderFlags_TooltipFollowMouse = 1 << 3,
    SliderFlags_TooltipFade = 1 << 4,
    SliderFlags_TooltipEase = 1 << 5,
    SliderFlags_TooltipScale = 1 << 6,
    SliderFlags_TooltipNoArrow = 1 << 7,
    SliderFlags_EnableClickSeek = 1 << 8,
    SliderFlags_NoNav = 1 << 9,
    SliderFlags_TooltipNoBorder = 1 << 10,
    SliderFlags_NoSeekOnClick = 1 << 11,
    SliderFlags_DisableSeek = 1 << 12,
    SliderFlags_EnableSmoothPreview = 1 << 13,
    SliderFlags_SliderEase = 1 << 14,
    SliderFlags_SliderFade = 1 << 15,
    SliderFlags_TooltipAnimation = SliderFlags_TooltipEase | SliderFlags_TooltipFade | SliderFlags_TooltipScale,
    SliderFlags_SliderpAnimation = SliderFlags_SliderEase | SliderFlags_SliderFade | SliderFlags_TooltipScale,
    SliderFlags_Animation = SliderFlags_TooltipAnimation | SliderFlags_SliderpAnimation,
    SliderFlags_TooltipDefault = SliderFlags_TooltipFollowMouse | SliderFlags_TooltipScale,
    SliderFlags_Default = SliderFlags_TooltipDefault | SliderFlags_EnableClickSeek,
};



struct SliderState {
    ImGuiID id;
    ImGuiID preview_id;
    ImGuiID tooltip_id;
    ImGuiID tooltip_anim_id;
    ImGuiID slider_id;
    ImGuiID slider_anim_id;

    ImGuiContext *g;
    ImDrawList *draw_list;
    ImFont *font;
    float fontsize;

    float anim_t;
    float display_v;
    float t;
    float dt;
    float range;
    float grab_radius;
    float width;
    float height;
    float height_max;
    float v_min;
    float v_max;

    ImVec2 pos;
    ImVec2 size;
    ImVec2 mouse;
    ImVec2 grab_center;

    bool hovered;
    bool grab_hovered;
    bool bar_hovered;
    bool active;

    struct SliderPreviewValue {
    private:
        float preview_value = 0.0f;
        bool is_previewing = false;

    public:
        float GetDisplayValue(float actual_v) const { return is_previewing ? preview_value : actual_v; }
        bool IsPreviewing() const { return is_previewing; }
        float GetPreviewValue() const { return preview_value; }

        void BeginPreview(float v) {
            preview_value = v;
            is_previewing = true;
        }
        void UpdatePreview(float v) { preview_value = v; }
        void EndPreview() { is_previewing = false; }
    } *preview = nullptr;
};

struct SliderSeekRequest {
    float new_value;
    bool from_click;
    bool from_drag;
    bool is_hovered;
    bool is_final;
};

struct SliderSeekResult {
    bool accept = true;
    float value = 0.0f;
};

using SliderRenderCallback = std::function<void(Phase phase, Slot slot, SliderState *state, SliderRenderData *data, ImDrawList *draw_list)>;
using SliderTooltipCallback = std::function<void(Phase phase, Slot slot, TooltipData *data, ImDrawList *draw_list)>;
using SliderSeekCallback = std::function<SliderSeekResult(const SliderSeekRequest *)>;
using SliderNavCallback = std::function<void(float value, bool nav_left, bool nav_right)>;

namespace CSImGui {
    bool ModernSliderFloatEx(const char *label, float *v, float v_min, float v_max,
                             float height = 4.0f, float grab_radius = 8.0f,
                             const char *format = "%.3f", float custom_width = -1.0f,
                             SliderFlags flags = SliderFlags_None,
                             SliderRenderCallback render_cb = nullptr,
                             SliderTooltipCallback tooltip_cb = nullptr,
                             SliderSeekCallback seek_cb = nullptr,
                             SliderNavCallback nav_cb = nullptr);

    bool ModernSliderFloat(const char *label, float *v, float v_min, float v_max,
                           float height = 4.0f, float grab_radius = 8.0f,
                           const char *format = "%.3f", float custom_width = -1.0f,
                           SliderFlags flags = SliderFlags_None);
}