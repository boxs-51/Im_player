#include "gui_slider.h"
#include "../core/gui_theme.h"
#include <utils.h>
#include <imgui_internal.h>

static void SliderRenderBar(float* v, SliderState* state, SliderRenderData* rd, SliderFlags flags = SliderFlags_None, SliderRenderCallback render_cb = nullptr) {
    bool is_expanding = rd->bar_hovered || rd->active || rd->grab_hovered;
    float visual_height = state->height + rd->height_on_hover; 
    if (flags & SliderFlags_SliderEase && rd->anim) {
        rd->anim->bar_height_scale = ImLerp(rd->anim->bar_height_scale, is_expanding ? 1.0f : 0.0f, SMOOTH_LERP(rd->anim->speedease, state->dt));
        visual_height = state->height + (rd->height_on_hover * rd->anim->bar_height_scale);
    }
    if (visual_height > state->height_max) visual_height = state->height_max;

    ImVec2 p1(state->pos.x, state->grab_center.y - visual_height * 0.5f);
    ImVec2 p2(state->pos.x + state->size.x, state->grab_center.y + visual_height * 0.5f);
    ImVec2 fill_p2(p1.x + state->anim_t * state->size.x, p2.y);

    rd->track_p1 = p1;
    rd->track_p2 = p2;
    rd->fill_p2  = fill_p2;
    rd->grab_center = state->grab_center;

    rd->height = state->height;
    rd->grab_radius = state->grab_radius;
    rd->track_height = visual_height;

    rd->value = *v;
    rd->t = state->t;
    rd->visual_t = state->anim_t;

    rd->active = state->active;
    rd->hovered = state->hovered;
    rd->bar_hovered = state->bar_hovered;
    rd->grab_hovered = state->grab_hovered;

    rd->col_track = state->bar_hovered ? ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered) : ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    rd->col_fill = ImGui::GetStyleColorVec4(rd->active ? ImGuiCol_PlotHistogramActive : ImGuiCol_PlotHistogram);
    rd->col_border = ImGui::GetStyleColorVec4(ImGuiCol_Border);
    rd->col_grab = rd->active ? ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabActive) : rd->grab_hovered ? ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabHovered) : ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    rd->col_grab_border = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    rd->col_buffer = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
    rd->col_marker = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    rd->col_chapter_range = ImGui::GetStyleColorVec4(ImGuiCol_Text);

    if (flags & SliderFlags_SliderEase && rd->anim) {
        rd->anim->hover = ImLerp(rd->anim->hover, rd->hovered ? 1.0f : 0.0f, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->active = ImLerp(rd->anim->active, rd->active ? 1.0f : 0.0f, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->grab_hover = ImLerp(rd->anim->grab_hover, rd->grab_hovered ? 1.0f : 0.0f, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->grab_scale = rd->active ? 1.3f : rd->grab_hovered ? 1.15f : 1.0f;
    }
    if (render_cb) render_cb(Phase::Init, Slot::None, state, rd, nullptr);

    ImVec4 col_track = rd->col_track;
    ImVec4 col_fill = rd->col_fill;
    ImVec4 col_grab = rd->col_grab;
    ImVec4 col_buffer = rd->col_buffer;
    ImVec4 col_border = rd->col_border;
    ImVec4 col_grab_border = rd->col_grab_border;
    ImVec4 col_marker = rd->col_marker;
    ImVec4 col_chapter_range = rd->col_chapter_range;
    ImVec4 col_grab_shadow = rd->col_grab_shadow;

    float grab_scale = rd->active ? 1.3f : rd->grab_hovered ? 1.15f : 1.0f;
    if (flags & SliderFlags_SliderEase && rd->anim) {
        rd->anim->col_track = ImLerp(rd->anim->col_track, rd->col_track, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_fill = ImLerp(rd->anim->col_fill, rd->col_fill, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_grab = ImLerp(rd->anim->col_grab, rd->col_grab, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_buffer = ImLerp(rd->anim->col_buffer, rd->col_buffer, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_border = ImLerp(rd->anim->col_border, rd->col_border, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_grab_border = ImLerp(rd->anim->col_grab_border, rd->col_grab_border, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_marker = ImLerp(rd->anim->col_marker, rd->col_marker, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_chapter_range = ImLerp(rd->anim->col_chapter_range, rd->col_chapter_range, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_grab_shadow = ImLerp(rd->anim->col_grab_shadow, rd->col_grab_shadow, SMOOTH_LERP(rd->anim->speedease, state->dt));
        
        rd->anim->grab_scale = 1.0f + 0.3f * rd->anim->active + 0.15f * rd->anim->grab_hover;

        col_track = rd->anim->col_track;
        col_fill = rd->anim->col_fill;
        col_grab = rd->anim->col_grab;
        col_buffer = rd->anim->col_buffer;
        col_border = rd->anim->col_border;
        col_grab_border = rd->anim->col_grab_border;
        col_marker = rd->anim->col_marker;
        col_chapter_range = rd->anim->col_chapter_range;
        col_grab_shadow = rd->anim->col_grab_shadow;

        grab_scale = rd->anim->grab_scale;
    }

    float radius = rd->grab_radius * grab_scale;
    radius = ImClamp(radius, 1.0f, state->height_max * 0.5f);
    if (render_cb) render_cb(Phase::Draw, Slot::Draw_layer0, state, rd, state->draw_list);

    if (!rd->skip_draw_track && !rd->skip_draw) {
        state->draw_list->AddRectFilled(rd->track_p1, rd->track_p2, ToCol32(col_track), rd->track_height * 0.5f);
    }
    if (!rd->skip_draw_buffer && rd->buffer_t >= 0.0f && !rd->skip_draw) {
        float bt = ImClamp(rd->buffer_t, 0.0f, 1.0f);
        float x = ImLerp(rd->track_p1.x, rd->track_p2.x, bt);
        state->draw_list->AddRectFilled(rd->track_p1, ImVec2(x, rd->track_p2.y), ToCol32(rd->col_buffer), rd->track_height * 0.5f);
    }
    if (rd->chapter_range_end > rd->chapter_range_start && rd->chapter_range_start >= 0.0f && !rd->skip_draw_chapter_range && !rd->skip_draw) {
        float x1 = ImLerp(rd->track_p1.x, rd->track_p2.x, rd->chapter_range_start);
        float x2 = ImLerp(rd->track_p1.x, rd->track_p2.x, rd->chapter_range_end);
        state->draw_list->AddRectFilled(ImVec2(x1, rd->track_p1.y), ImVec2(x2, rd->track_p2.y), ToCol32(rd->col_chapter_range), rd->track_height * 0.5f);
    }
    if (!rd->skip_draw_fill && !rd->skip_draw) {
        state->draw_list->AddRectFilled(rd->track_p1, rd->fill_p2, ToCol32(col_fill), rd->track_height * 0.5f);
    }
    if (render_cb) render_cb(Phase::Draw, Slot::Draw_layer1, state, rd, state->draw_list);
    
    if (!rd->skip_draw_markers && !rd->markers.empty() && !rd->skip_draw) {
        for (float m : rd->markers) {
            float mt = ImClamp(m, 0.0f, 1.0f);
            float x = ImLerp(rd->track_p1.x, rd->track_p2.x, mt);
            state->draw_list->AddLine(ImVec2(x, rd->track_p1.y), ImVec2(x, rd->track_p2.y), ToCol32(rd->col_marker), rd->marker_thickness);
        }
    }
    if (!rd->skip_draw_track && !rd->skip_draw_border && !rd->skip_draw) {
        state->draw_list->AddRect(rd->track_p1, rd->track_p2, ToCol32(rd->col_border), rd->track_height * 0.5f);
    }
    if (!rd->skip_draw_grab && !rd->skip_draw) {
        state->draw_list->AddCircleFilled(rd->grab_center, rd->grab_shadow_size, ToCol32(rd->col_grab_shadow));
        state->draw_list->AddCircleFilled(rd->grab_center, radius, ToCol32(col_grab));
        state->draw_list->AddCircle(rd->grab_center, radius, ToCol32(rd->col_grab_border), 0, 1.5f);
    }
}

bool CSImGui::ModernSliderFloatEx(const char* label, float* v,
    float v_min, float v_max,
    float height, float grab_radius,
    const char* format, float custom_width,
    SliderFlags flags,
    SliderRenderCallback render_cb,
    SliderTooltipCallback tooltip_cb,
    SliderSeekCallback seek_cb,
    SliderNavCallback nav_cb
) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    SliderState* state = (SliderState*)window->StateStorage.GetVoidPtr(window->GetID(label));
    if (!state) {
        state = (SliderState*)IM_ALLOC(sizeof(SliderState));
        IM_PLACEMENT_NEW(state) SliderState();
        window->StateStorage.SetVoidPtr(window->GetID(label), state);
        state->id = window->GetID(label);
    }
    
    if (flags & SliderFlags_EnableSmoothPreview) {
        state->preview_id = ImHashData(&state->id, sizeof(ImGuiID), 0xA4158443);
        state->preview = (SliderState::SliderPreviewValue*)window->StateStorage.GetVoidPtr(state->preview_id);
        if (!state->preview) {
            state->preview = (SliderState::SliderPreviewValue*)IM_ALLOC(sizeof(SliderState::SliderPreviewValue));
            IM_PLACEMENT_NEW(state->preview) SliderState::SliderPreviewValue();
            window->StateStorage.SetVoidPtr(state->preview_id, state->preview);
        }
    }

    if (window->SkipItems) return false;

    state->g = GImGui;
    state->draw_list = window->DrawList;
    state->font = ImGui::GetFont();
    state->fontsize = ImGui::GetFontSize();

    const char* label_end = ImGui::FindRenderedTextEnd(label);
    if (label_end != label) {
        ImGui::TextDisabled("%.*s: %s", (int)(label_end - label), label, TextUtils::Format(format, *v).c_str());
    }

    state->width = (custom_width > 0) ? custom_width : ImGui::CalcItemWidth();
    state->range = (v_max - v_min);
    state->height = height;
    if (state->range <= 0.0f) state->range = 1.0f;
    state->grab_radius = grab_radius;
    state->height_max = state->grab_radius * 2.5f;
    if (state->height > state->height_max) state->height = state->height_max;
    
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(state->width, state->height_max);
    state->pos = window->DC.CursorPos;
    state->size = ImVec2(state->width, state->height_max);
    ImRect bb(pos, pos + size);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, state->id)) return false;

    bool is_disabled = (state->g->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    
    state->hovered = is_disabled ? false : ImGui::ItemHoverable(bb, state->id, ImGuiItemFlags_None);
    state->active = !is_disabled && (state->g->ActiveId == state->id);
    if (flags & SliderFlags_EnableSmoothPreview)
        state->display_v = state->preview->GetDisplayValue(*v);
    else
        state->display_v = *v;

    float dt = state->g->IO.DeltaTime;
    state->dt = state->g->IO.DeltaTime;
    state->v_min = v_min;
    state->v_max = v_max;

    if (flags & SliderFlags_SliderEase) {
        state->anim_t = ImLerp(state->anim_t, state->t, SMOOTH_LERP(12.0f, dt));
    } else {
        state->anim_t = state->t;
    }
    state->grab_center = ImVec2(pos.x + state->anim_t * size.x, pos.y + size.y * 0.5f);

    state->mouse = state->g->IO.MousePos;
    float dx = state->mouse.x - state->grab_center.x;
    float dy = state->mouse.y - state->grab_center.y;

    float r = state->grab_radius * 1.5f;
    state->grab_hovered = state->hovered && (dx * dx + dy * dy <= r * r);
    state->bar_hovered  = state->hovered && !state->grab_hovered;

    bool changed = false;
    float new_v = state->display_v;
    bool is_interacting = false;
    bool is_click_frame = false;

    if (state->hovered && ImGui::IsMouseClicked(0)) {
        ImGui::SetActiveID(state->id, window);
        ImGui::FocusWindow(window);
        is_click_frame = true;

        if (!(flags & SliderFlags_DisableSeek)) {
            float t_curr = (*v - v_min) / state->range;
            float grab_x = pos.x + t_curr * size.x;
            float dx = state->mouse.x - grab_x;
            float dy = state->mouse.y - state->grab_center.y;
            
            bool is_grab_click = (dx*dx + dy*dy <= state->grab_radius * state->grab_radius * 2.25f);

            if (is_grab_click) {
                state->g->ActiveIdClickOffset = ImVec2(dx, 0.0f);
            } else if (flags & SliderFlags_EnableClickSeek) {
                state->g->ActiveIdClickOffset.x = 0.0f;
                float t_mouse = ImClamp((state->mouse.x - pos.x) / size.x, 0.0f, 1.0f);
                new_v = v_min + t_mouse * state->range;
            }
        }
    }

    if (state->active) {
        is_interacting = true;
        if (state->g->ActiveIdSource == ImGuiInputSource_Mouse) {
            float mouse_x = state->mouse.x - state->g->ActiveIdClickOffset.x;
            float t = ImClamp((mouse_x - pos.x) / size.x, 0.0f, 1.0f);
            new_v = v_min + t * state->range;
        }

        if (!ImGui::IsMouseDown(0))
            ImGui::ClearActiveID();
    }

    if (is_interacting && !(flags & SliderFlags_DisableSeek)) {
        bool is_final = !ImGui::IsMouseDown(0);

        if (seek_cb) {
            SliderSeekRequest req{};
            req.new_value  = new_v;
            req.from_click = is_click_frame;
            req.from_drag  = !is_click_frame;
            req.is_final   = is_final; 
            req.is_hovered = state->hovered;

            if (state->preview) state->preview->BeginPreview(new_v);

            auto res = seek_cb(&req);
            if (res.accept) {
                if (*v != res.value) {
                    *v = res.value;
                    if (state->preview) state->preview->EndPreview();
                    changed = true;
                }
            }
        } else if (new_v != *v) {
            *v = new_v;
            changed = true;
        }
    }

    if (!is_disabled) {
        if (state->g->NavId == state->id && state->g->NavActivatePressedId == state->id)
            ImGui::SetActiveID(state->id, window);

        if (state->active && state->g->NavId == state->id && state->g->NavInputSource == ImGuiInputSource_Keyboard) {
            bool left  = ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
            bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow);

            if (nav_cb) {
                if ((left || right)) nav_cb(*v, left, right);
            } else if (!(flags & SliderFlags_NoNav)) {
                float step = state->range * 0.01f;
                float old = *v;

                if (left)  *v -= step;
                if (right) *v += step;

                *v = ImClamp(*v, v_min, v_max);
                if (*v != old) changed = true;
            }
        }
    }

    state->t = (state->display_v - v_min) / state->range;

    state->tooltip_id = ImHashData(&state->id, sizeof(ImGuiID), 0xA546823);
    TooltipData* td = (TooltipData*)window->StateStorage.GetVoidPtr(state->tooltip_id);
    if (!td) {
        td = (TooltipData*)IM_ALLOC(sizeof(TooltipData));
        IM_PLACEMENT_NEW(td) TooltipData();
        window->StateStorage.SetVoidPtr(state->tooltip_id, td);
    }
    if ((flags & SliderFlags_TooltipFade) && (flags & SliderFlags_TooltipEase)) {
        state->tooltip_anim_id = ImHashData(&state->tooltip_id, sizeof(ImGuiID), 0xA444682);
        td->anim = (TooltipAnimState*)window->StateStorage.GetVoidPtr(state->tooltip_anim_id);
        if (!td->anim) {
            td->anim = (TooltipAnimState*)IM_ALLOC(sizeof(TooltipAnimState));
            IM_PLACEMENT_NEW(td->anim) TooltipAnimState();
            window->StateStorage.SetVoidPtr(state->tooltip_anim_id, td->anim);
        }
    }

    td->item.value = *v;
    td->item.v_min = v_min;
    td->item.range = state->range;
    td->config.dt = state->dt;
    td->item.pos = state->pos;
    td->item.size = state->size;
    td->item.mouse = state->mouse;
    td->item.hovered_time = state->g->HoveredIdTimer;
    td->item.active = state->active;
    td->item.hovered = state->hovered;
    td->config.font = state->font;
    td->config.fontsize = state->fontsize;
    td->item.center.y = state->grab_center.y - state->grab_radius;
    td->config.format = format;
    td->config.id = state->id;

    ToolTipFlags tdflags = ToolTipFlags_ClampItem;
    if (flags & SliderFlags_TooltipAlwaysShowAction) tdflags |= ToolTipFlags_AlwaysShowAction;
    if (flags & SliderFlags_TooltipAlwaysShow) tdflags |= ToolTipFlags_AlwaysShow;
    if (flags & SliderFlags_TooltipAnimation) tdflags |= ToolTipFlags_Animation;
    if (flags & SliderFlags_TooltipEase) tdflags |= ToolTipFlags_Ease;
    if (flags & SliderFlags_TooltipFade) tdflags |= ToolTipFlags_Fade;
    if (flags & SliderFlags_TooltipFollowMouse) tdflags |= ToolTipFlags_FollowMouse_Fixed_Y;
    if (flags & SliderFlags_TooltipHiden) tdflags |= ToolTipFlags_Hiden;
    if (flags & SliderFlags_TooltipNoArrow) tdflags |= ToolTipFlags_NoArrow;
    if (flags & SliderFlags_TooltipNoBorder) tdflags |= ToolTipFlags_NoBorder;
    if (flags & SliderFlags_TooltipScale) tdflags |= ToolTipFlags_Scale;
    
    ToolTipEx(td, tdflags, tooltip_cb);

    state->slider_id = ImHashData(&state->id, sizeof(ImGuiID), 0xA1234567);
    SliderRenderData* rd = (SliderRenderData*)window->StateStorage.GetVoidPtr(state->slider_id);
    if (!rd) {
        rd = (SliderRenderData*)IM_ALLOC(sizeof(SliderRenderData));
        IM_PLACEMENT_NEW(rd) SliderRenderData();
        window->StateStorage.SetVoidPtr(state->slider_id, rd);
    }
    if (flags & SliderFlags_SliderEase || flags & SliderFlags_SliderFade) {
        state->slider_anim_id  = ImHashData(&state->slider_id, sizeof(ImGuiID), 0xB7654321);
        rd->anim = (SliderRenderData::SliderAnimState*)window->StateStorage.GetVoidPtr(state->slider_anim_id);
        if (!rd->anim) {
            rd->anim = (SliderRenderData::SliderAnimState*)IM_ALLOC(sizeof(SliderRenderData::SliderAnimState));
            IM_PLACEMENT_NEW(rd->anim) SliderRenderData::SliderAnimState();
            window->StateStorage.SetVoidPtr(state->slider_anim_id, rd->anim);
        }
    }
    if (rd) SliderRenderBar(v, state, rd, flags, render_cb);

    return changed;
}

bool CSImGui::ModernSliderFloat(const char* label, float* v,
    float v_min, float v_max, 
    float height, float grab_radius, 
    const char* format, float custom_width,
    SliderFlags flag) 
{
    ImGui::PushStyleColor(ImGuiCol_PopupBg,             GetColors(Col_PopupBg));
    ImGui::PushStyleColor(ImGuiCol_Border,              GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Text,                GetColors(Col_Text));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,       GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,              GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram,        GetColors(Col_PlotHistogram));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogramActive,  GetColors(Col_PlotHistogramActive));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabHovered,    GetColors(Col_SliderGrabHovered));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,     GetColors(Col_SliderGrabActive));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,           GetColors(Col_SliderGrab));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,            GetColors(Col_CheckMark));
    
    SliderFlags flag_ex = SliderFlags_Default | SliderFlags_Animation | flag;
    bool s = ModernSliderFloatEx(label, v, v_min, v_max, height, grab_radius, format, custom_width, flag_ex);
                 
    ImGui::PopStyleColor(11);
    return s;
}

bool CSImGui::ModernProgressBarEx(const char* label, float* v, float buffer_v,
                         float v_min, float v_max,
                         float height, const char* overlay_text,
                         float custom_width, ProgressBarFlags flags) 
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiID id = window->GetID(label);
    ProgressBarState* state = (ProgressBarState*)window->StateStorage.GetVoidPtr(id);
    if (!state) {
        state = (ProgressBarState*)IM_ALLOC(sizeof(ProgressBarState));
        IM_PLACEMENT_NEW(state) ProgressBarState();
        window->StateStorage.SetVoidPtr(id, state);
        state->id = id;
    }

    state->g = GImGui;
    state->draw_list = window->DrawList;
    state->font = ImGui::GetFont();
    state->fontsize = ImGui::GetFontSize();
    state->dt = state->g->IO.DeltaTime;
    state->v_min = v_min;
    state->v_max = v_max;
    state->range = (v_max - v_min > 0.0f) ? (v_max - v_min) : 1.0f;

    state->width = (custom_width > 0.0f) ? custom_width : ImGui::CalcItemWidth();
    state->height = height;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(state->width, state->height);
    state->pos = pos;
    state->size = size;

    ImRect bb(pos, pos + size);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, state->id)) return false;

    bool is_disabled = (state->g->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    
    // Tương tác Mouse nếu bật ProgressBarFlags_Interactive
    bool is_interactive = (flags & ProgressBarFlags_Interactive) && !is_disabled;
    state->hovered = !is_disabled && ImGui::ItemHoverable(bb, state->id, ImGuiItemFlags_None);
    
    bool changed = false;
    if (is_interactive) {
        if (state->hovered && ImGui::IsMouseClicked(0)) {
            ImGui::SetActiveID(state->id, window);
            ImGui::FocusWindow(window);
        }

        state->active = (state->g->ActiveId == state->id);
        if (state->active) {
            if (ImGui::IsMouseDown(0)) {
                float mouse_x = state->g->IO.MousePos.x;
                float t = ImClamp((mouse_x - pos.x) / size.x, 0.0f, 1.0f);
                float new_v = v_min + t * state->range;
                if (*v != new_v) {
                    *v = new_v;
                    changed = true;
                }
            } else {
                ImGui::ClearActiveID();
            }
        }
    } else {
        state->active = false;
    }

    // Animation & Lerp Progress
    state->t = ImClamp((*v - v_min) / state->range, 0.0f, 1.0f);
    float buffer_t = ImClamp((buffer_v - v_min) / state->range, 0.0f, 1.0f);

    if (flags & ProgressBarFlags_Ease) {
        state->anim_t = ImLerp(state->anim_t, state->t, ImClamp(12.0f * state->dt, 0.0f, 1.0f));
    } else {
        state->anim_t = state->t;
    }

    // Allocation Render Data
    state->render_data_id = ImHashData(&state->id, sizeof(ImGuiID), 0x87654321);
    ProgressBarRenderData* rd = (ProgressBarRenderData*)window->StateStorage.GetVoidPtr(state->render_data_id);
    if (!rd) {
        rd = (ProgressBarRenderData*)IM_ALLOC(sizeof(ProgressBarRenderData));
        IM_PLACEMENT_NEW(rd) ProgressBarRenderData();
        window->StateStorage.SetVoidPtr(state->render_data_id, rd);
    }

    if (flags & ProgressBarFlags_Ease) {
        state->anim_id = ImHashData(&state->render_data_id, sizeof(ImGuiID), 0x12345678);
        rd->anim = (ProgressBarRenderData::AnimState*)window->StateStorage.GetVoidPtr(state->anim_id);
        if (!rd->anim) {
            rd->anim = (ProgressBarRenderData::AnimState*)IM_ALLOC(sizeof(ProgressBarRenderData::AnimState));
            IM_PLACEMENT_NEW(rd->anim) ProgressBarRenderData::AnimState();
            window->StateStorage.SetVoidPtr(state->anim_id, rd->anim);
        }
        rd->anim->buffer_t = ImLerp(rd->anim->buffer_t, buffer_t, ImClamp(12.0f * state->dt, 0.0f, 1.0f));
        buffer_t = rd->anim->buffer_t;
    }

    // Render Setup
    ImVec2 p1 = pos;
    ImVec2 p2 = ImVec2(pos.x + size.x, pos.y + size.y);
    ImVec2 fill_p2 = ImVec2(p1.x + state->anim_t * size.x, p2.y);
    ImVec2 buffer_p2 = ImVec2(p1.x + buffer_t * size.x, p2.y);

    ImVec4 col_track  = GetColors(Col_FrameBg);
    ImVec4 col_buffer = GetColors(Col_PopupBg);
    ImVec4 col_fill   = GetColors(state->active ? Col_PlotHistogramActive : Col_PlotHistogram);
    ImVec4 col_border = GetColors(Col_Border);
    ImVec4 col_text   = GetColors(Col_Text);

    // 1. Track (Nền)
    state->draw_list->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(col_track), height * 0.5f);

    // 2. Buffer (Thanh đệm)
    if ((flags & ProgressBarFlags_EnableBuffer) && buffer_t > 0.0f) {
        state->draw_list->AddRectFilled(p1, buffer_p2, ImGui::ColorConvertFloat4ToU32(col_buffer), height * 0.5f);
    }

    // 3. Fill (Tiến trình chính)
    state->draw_list->AddRectFilled(p1, fill_p2, ImGui::ColorConvertFloat4ToU32(col_fill), height * 0.5f);

    // 4. Border (Viền)
    state->draw_list->AddRect(p1, p2, ImGui::ColorConvertFloat4ToU32(col_border), height * 0.5f);

    // 5. Render Text Overlay
    char buf[64];
    const char* text_to_render = overlay_text;
    if (!text_to_render) {
        if (flags & ProgressBarFlags_ShowPercentage) {
            snprintf(buf, sizeof(buf), "%d%%", (int)(state->t * 100.0f));
            text_to_render = buf;
        } else if (flags & ProgressBarFlags_ShowValue) {
            snprintf(buf, sizeof(buf), "%.1f / %.1f", *v, v_max);
            text_to_render = buf;
        }
    }

    if (text_to_render && text_to_render[0] != '\0') {
        ImVec2 text_size = state->font->CalcTextSizeA(state->fontsize, FLT_MAX, 0.0f, text_to_render);
        ImVec2 text_pos = ImVec2(p1.x + (size.x - text_size.x) * 0.5f, p1.y + (size.y - text_size.y) * 0.5f);
        state->draw_list->AddText(state->font, state->fontsize, text_pos, ImGui::ColorConvertFloat4ToU32(col_text), text_to_render);
    }

    return changed;
}

bool CSImGui::ModernProgressBar(const char* label, float progress, float height, const char* overlay, float width, ProgressBarFlags flags) {
    float v = progress;
    return ModernProgressBarEx(label, &v, 0.0f, 0.0f, 1.0f, height, overlay, width, flags);
}