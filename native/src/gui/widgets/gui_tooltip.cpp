#include "gui_tooltip.h"
#include "../core/gui_theme.h"
#include "gui_slider.h"
#include <utils.h>
#include <cmath>
#include <imgui_internal.h>

static ImVec2 FitSizeWithAspect(ImVec2 size, float max_w, float max_h, float aspect) {
    if (size.x <= 0 || size.y <= 0) return size;
    float w = size.x;
    float h = size.y;
    if (w > max_w) {
        w = max_w;
        h = w / aspect;
    }
    if (h > max_h) {
        h = max_h;
        w = h * aspect;
    }
    return ImVec2(w, h);
}

static bool UpdateTooltipState(TooltipData* td, ToolTipFlags flags, float& out_alpha) {
    auto& item = td->item;
    auto& cfg  = td->config;
    auto  anim = td->anim;

    if (flags & ToolTipFlags_Hiden)
        item.is_visible = false;
    else if (flags & ToolTipFlags_AlwaysShow)
        item.is_visible = true;
    else if (flags & ToolTipFlags_AlwaysShowAction)
        item.is_visible = item.active;
    else
        item.is_visible = (item.hovered && item.hovered_time >= item.hover_delay);

    if (!item.is_visible) cfg.lock_dir = false;
    
    float target_alpha = item.is_visible ? 1.0f : 0.0f;
    out_alpha = target_alpha;
    
    if (anim) {
        if (flags & ToolTipFlags_Fade) {
            anim->alpha = ImLerp(anim->alpha, target_alpha, SMOOTH_LERP(anim->speedfade, cfg.dt));
            out_alpha = anim->alpha;
        } else {
            anim->alpha = target_alpha;
        }
    }
    return out_alpha > 0.01f;
}

static void ComputeTooltipLayout(TooltipData* td, ToolTipFlags flags, float alpha, TooltipCallback tooltip_cb) {
    auto& item = td->item;
    auto& cfg  = td->config;

    float seek_v = 0.0f;
    if (cfg.show_text) {
        float t_mouse = ImClamp((item.mouse.x - item.pos.x) / item.size.x, 0.0f, 1.0f);
        seek_v = item.v_min + t_mouse * item.range;
    }
    
    if (cfg.show_text)
        item.seek_value = seek_v;

    cfg.col_bg = ImGui::GetColorU32(ImGuiCol_PopupBg);
    cfg.col_text = ImGui::GetColorU32(ImGuiCol_Text);
    cfg.col_border = ImGui::GetColorU32(ImGuiCol_Border);
    cfg.col_title = ImGui::GetColorU32(ImGuiCol_CheckMark);
    cfg.col_extra = ImGui::GetColorU32(ImGuiCol_Header);

    if (cfg.show_text)
        cfg.text = TextUtils::Format(cfg.format, item.active ? item.value : item.seek_value);

    if (tooltip_cb) tooltip_cb(Phase::Init, Slot::None, td, NULL);

    cfg.col_bg = ImGui::GetColorU32(cfg.col_bg, alpha);
    cfg.col_text = ImGui::GetColorU32(cfg.col_text, alpha);
    cfg.col_border = ImGui::GetColorU32(cfg.col_border, alpha);
    cfg.col_title = ImGui::GetColorU32(cfg.col_title, alpha);
    cfg.col_extra = ImGui::GetColorU32(cfg.col_extra, alpha);

    float max_content_w = ImMax(1.0f, cfg.max_width - cfg.padding_content * 2);
    float max_content_h = ImMax(1.0f, cfg.max_height - cfg.padding_content * 2);

    if ((cfg.last_raw_title != cfg.title) && cfg.show_title) {
        cfg.cached_title = TextUtils::WrapTextToLines(cfg.font, cfg.fontsize, cfg.title, max_content_w, cfg.title_max_lines);
        cfg.last_raw_title = cfg.title;
    }
    if ((cfg.last_raw_extra != cfg.extra) && cfg.show_extra) {
        cfg.cached_extra = TextUtils::WrapTextToLines(cfg.font, cfg.fontsize, cfg.extra, max_content_w, cfg.title_max_lines);
        cfg.last_raw_extra = cfg.extra;
    }
    if ((cfg.last_raw_text != cfg.text) && cfg.show_text) {
        cfg.cached_text = TextUtils::WrapTextToLines(cfg.font, cfg.fontsize, cfg.text, max_content_w, cfg.title_max_lines);
        cfg.last_raw_text = cfg.text;
    }
    
    if (cfg.show_image) {
        if (cfg.lock_aspect) {
            cfg.image_size = FitSizeWithAspect(
                cfg.image_size,
                cfg.max_width,
                max_content_h * 0.6f,
                cfg.aspect_ratio
            );
        }
    } else {
        cfg.image_size = ImVec2(0, 0);
    }

    auto MeasureLines = [&](const std::vector<std::string>& lines) {
        ImVec2 res(0, 0);
        for (const auto& l : lines) {
            ImVec2 sz = cfg.font->CalcTextSizeA(cfg.fontsize, FLT_MAX, 0.0f, l.c_str());
            res.x = ImMax(res.x, sz.x);
            res.y += sz.y;
        }
        if (lines.size() > 1) res.y += (lines.size() - 1) * cfg.spacing_content; 
        return res;
    };

    if (cfg.show_title) cfg.title_size = MeasureLines(cfg.cached_title);
    if (cfg.show_extra) cfg.extra_size = MeasureLines(cfg.cached_extra);
    if (cfg.show_text)  cfg.text_size  = MeasureLines(cfg.cached_text);

    float content_w = 0.0f;
    float content_h = 0.0f;

    if (cfg.layout == ImGuiTooltip::Layout_Vertical) {
        content_w = cfg.text_size.x;
        if (cfg.show_title) content_w = ImMax(content_w, cfg.title_size.x);
        if (cfg.show_extra) content_w = ImMax(content_w, cfg.extra_size.x);
        if (cfg.show_image) content_w = ImMax(content_w, cfg.image_size.x);

        if (cfg.show_image) content_h += cfg.image_size.y + cfg.spacing_content;
        if (cfg.show_title) content_h += cfg.title_size.y + cfg.spacing_content;
        content_h += cfg.text_size.y; 
        if (cfg.show_extra) content_h += cfg.spacing_content + cfg.extra_size.y;
    } else {
        float text_block_h = cfg.text_size.y;
        float text_block_w = cfg.text_size.x;
        
        if (cfg.show_title) {
            text_block_h += cfg.title_size.y + cfg.spacing_content;
            text_block_w = ImMax(text_block_w, cfg.title_size.x);
        }
        if (cfg.show_extra) {
            text_block_h += cfg.extra_size.y + cfg.spacing_content;
            text_block_w = ImMax(text_block_w, cfg.extra_size.x);
        }

        content_w = text_block_w;
        content_h = text_block_h;

        if (cfg.show_image) {
            content_w += cfg.image_size.x + cfg.spacing_content;
            content_h = ImMax(content_h, cfg.image_size.y);
        }
    }

    content_w = ImMin(content_w, max_content_w);
    content_h = ImMin(content_h, max_content_h);
    content_w = ImMax(content_w, 1.0f);
    content_h = ImMax(content_h, 1.0f);

    td->out_size = ImVec2(
        content_w + cfg.padding_content * 2,
        content_h + cfg.padding_content * 2
    );
}

static void ComputeTooltipPosition(TooltipData* td, ToolTipFlags flags) {
    auto& item = td->item;
    auto& cfg  = td->config;

    ImVec2 view_min = ImGui::GetMainViewport()->Pos;
    ImVec2 view_max = view_min + ImGui::GetMainViewport()->Size;
    ImVec2 limit_min = view_min;
    ImVec2 limit_max = view_max;

    auto GetDirVector = [&](ImGuiTooltip::TooltipDirection dir) {
        switch (dir) {
        case ImGuiTooltip::Dir_Up:        return ImVec2(0, -1);
        case ImGuiTooltip::Dir_Down:      return ImVec2(0, 1);
        case ImGuiTooltip::Dir_Left:      return ImVec2(-1, 0);
        case ImGuiTooltip::Dir_Right:     return ImVec2(1, 0);
        case ImGuiTooltip::Dir_UpLeft:    return ImVec2(-1, -1);
        case ImGuiTooltip::Dir_UpRight:   return ImVec2(1, -1);
        case ImGuiTooltip::Dir_DownLeft:  return ImVec2(-1, 1);
        case ImGuiTooltip::Dir_DownRight: return ImVec2(1, 1);
        }
        return ImVec2(0, -1);
    };

    auto ComputeBestPosition = [&](ImVec2 target, ImVec2& out_pos, ImGuiTooltip::TooltipDirection& out_dir) {
        ImVec2 item_min = item.pos;
        ImVec2 item_max = item.pos + item.size;

        struct Candidate {
            ImVec2 pos;
            ImGuiTooltip::TooltipDirection dir;
            float score;
        };

        Candidate best = {};
        best.score = -FLT_MAX;

        auto Test = [&](ImVec2 pos, ImGuiTooltip::TooltipDirection dir) {
            ImVec2 pmax = pos + td->out_size;

            float overflow =
                ImMax(0.0f, view_min.x - pos.x) +
                ImMax(0.0f, view_min.y - pos.y) +
                ImMax(0.0f, pmax.x - view_max.x) +
                ImMax(0.0f, pmax.y - view_max.y);

            bool overlap =
                !(pmax.x < item_min.x || pos.x > item_max.x ||
                pmax.y < item_min.y || pos.y > item_max.y);

            float score = -overflow * 10.0f;
            if (!overlap) score += 1000.0f;

            if (score > best.score) {
                best = {pos, dir, score};
            }
        };

        Test(ImVec2(target.x - td->out_size.x * 0.5f, item_min.y - td->out_size.y - cfg.spacing_mouse), ImGuiTooltip::Dir_Up);
        Test(ImVec2(target.x - td->out_size.x * 0.5f, item_max.y + cfg.spacing_mouse), ImGuiTooltip::Dir_Down);
        Test(ImVec2(item_max.x + cfg.spacing_mouse, target.y - td->out_size.y * 0.5f), ImGuiTooltip::Dir_Right);
        Test(ImVec2(item_min.x - td->out_size.x - cfg.spacing_mouse, target.y - td->out_size.y * 0.5f), ImGuiTooltip::Dir_Left);
        
        Test(ImVec2(item_min.x - td->out_size.x - cfg.spacing_mouse, item_min.y - td->out_size.y), ImGuiTooltip::Dir_UpLeft);
        Test(ImVec2(item_max.x + cfg.spacing_mouse, item_min.y - td->out_size.y), ImGuiTooltip::Dir_UpRight);
        Test(ImVec2(item_min.x - td->out_size.x - cfg.spacing_mouse, item_max.y), ImGuiTooltip::Dir_DownLeft);
        Test(ImVec2(item_max.x + cfg.spacing_mouse, item_max.y), ImGuiTooltip::Dir_DownRight);

        out_pos = best.pos;
        out_dir = best.dir;
    };

    ImVec2 base_pos;
    ImGuiTooltip::TooltipDirection base_dir = ImGuiTooltip::Dir_Up;

    if (flags & ToolTipFlags_Fixed) {
        base_dir = ImGuiTooltip::Dir_Up;
        base_pos = ImVec2(
            item.center.x - td->out_size.x * 0.5f,
            item.center.y - td->out_size.y - cfg.spacing_mouse
        );
        cfg.lock_dir = false;
    } else if (flags & ToolTipFlags_AutoPosition) {
        ImVec2 target = item.mouse;

        if (flags & ToolTipFlags_FollowMouse_Fixed_X) target.x = item.center.x;
        if (flags & ToolTipFlags_FollowMouse_Fixed_Y) target.y = item.center.y;

        if (!cfg.lock_dir) {
            ImVec2 tmp_pos;
            ImGuiTooltip::TooltipDirection tmp_dir;

            ComputeBestPosition(target, tmp_pos, tmp_dir);

            cfg.dir = tmp_dir;
            cfg.lock_dir = true;
        }

        ImVec2 dir_vec = GetDirVector(cfg.dir);
        float len = sqrtf(dir_vec.x * dir_vec.x + dir_vec.y * dir_vec.y);
        float safe_pad = 6.0f;
        ImVec2 safe = item.cursor_size + ImVec2(safe_pad, safe_pad);
        if (len > 0.0f) dir_vec /= len;
        
        ImVec2 extra(0,0);
        if (dir_vec.x > 0) extra.x += safe.x;
        if (dir_vec.x < 0) extra.x -= safe.x;
        if (dir_vec.y > 0) extra.y += safe.y;
        if (dir_vec.y < 0) extra.y -= safe.y;

        ImVec2 size_offset(
            (dir_vec.x == 0 ? -td->out_size.x * 0.5f : (dir_vec.x < 0 ? -td->out_size.x : 0)),
            (dir_vec.y == 0 ? -td->out_size.y * 0.5f : (dir_vec.y < 0 ? -td->out_size.y : 0))
        );
        base_pos = target + dir_vec * cfg.spacing_mouse + extra + size_offset;
        base_dir = cfg.dir;
    } else {
        ImVec2 target = item.mouse;

        if (flags & ToolTipFlags_FollowMouse_Fixed_X) target.x = item.center.x;
        if (flags & ToolTipFlags_FollowMouse_Fixed_Y) target.y = item.center.y;

        base_pos = target - ImVec2(td->out_size.x * 0.5f, td->out_size.y + cfg.spacing_mouse);
        base_dir = ImGuiTooltip::Dir_Up;

        cfg.lock_dir = false;
    }

    td->out_pos = base_pos;
    cfg.dir = base_dir;

    if (flags & ToolTipFlags_ClampItem) {
        limit_min = item.pos;
        limit_max = item.pos + item.size;

        if (item.size.x < td->out_size.x) {
            float cx = item.pos.x + item.size.x * 0.5f;
            limit_min.x = cx - td->out_size.x * 0.5f;
            limit_max.x = cx + td->out_size.x * 0.5f;
        }
        if (item.size.y < td->out_size.y) {
            float cy = item.pos.y + item.size.y * 0.5f;
            limit_min.y = cy - td->out_size.y * 0.5f;
            limit_max.y = cy + td->out_size.y * 0.5f;
        }
    }
    bool is_x_fixed = (flags & ToolTipFlags_Fixed) || (flags & ToolTipFlags_FollowMouse_Fixed_X);
    bool is_y_fixed = (flags & ToolTipFlags_Fixed) || (flags & ToolTipFlags_FollowMouse_Fixed_Y);

    if (flags & ToolTipFlags_ClampItem) {
        if (!is_x_fixed) 
            td->out_pos.x = ImClamp(td->out_pos.x, limit_min.x + cfg.padding, limit_max.x - td->out_size.x - cfg.padding);
        
        if (!is_y_fixed)
            td->out_pos.y = ImClamp(td->out_pos.y, limit_min.y + cfg.padding, limit_max.y - td->out_size.y - cfg.padding);
    }
    
    if (flags & ToolTipFlags_ClampWindow) {
        td->out_pos.x = ImClamp(td->out_pos.x, view_min.x + cfg.padding, view_max.x - td->out_size.x - cfg.padding);
        td->out_pos.y = ImClamp(td->out_pos.y, view_min.y + cfg.padding, view_max.y - td->out_size.y - cfg.padding);
    }
}

static bool BeginTooltipWindow(TooltipData* td, const ImVec2& draw_pos, const ImVec2& draw_size) {
    auto& cfg = td->config;

    ImGui::SetNextWindowPos(draw_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(draw_size, ImGuiCond_Always);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar 
                                  | ImGuiWindowFlags_NoResize 
                                  | ImGuiWindowFlags_NoMove 
                                  | ImGuiWindowFlags_NoSavedSettings 
                                  | ImGuiWindowFlags_AlwaysAutoResize 
                                  | ImGuiWindowFlags_Tooltip 
                                  | ImGuiWindowFlags_NoBackground
                                  | ImGuiWindowFlags_NoScrollbar
                                  | ImGuiWindowFlags_NoScrollWithMouse;

    char window_name[64];
    ImFormatString(window_name, sizeof(window_name), "##custom_tooltip_%08X", cfg.id);

    if (!ImGui::Begin(window_name, NULL, window_flags)) {
        ImGui::End();
        return false;
    }

    cfg.draw_list = ImGui::GetWindowDrawList();
    
    ImGuiWindow* current_window = ImGui::GetCurrentWindow();
    current_window->Pos = draw_pos;
    current_window->Size = draw_size;
    current_window->SizeFull = draw_size;
    current_window->DC.CursorStartPos = draw_pos + ImVec2(cfg.padding_content, cfg.padding_content);
    
    current_window->DrawList->PushClipRect(
        ImVec2(draw_pos.x - 50.0f, draw_pos.y - 50.0f), 
        ImVec2(draw_pos.x + draw_size.x + 50.0f, draw_pos.y + draw_size.y + 50.0f)
    );

    return true;
}

static void RenderTooltipBackground(TooltipData* td, ToolTipFlags flags, float scale, const ImVec2& center, const ImVec2& pmin, const ImVec2& pmax) {
    auto& item = td->item;
    auto& cfg  = td->config;
    auto  anim = td->anim;

    if (cfg.skip_draw) return;

    float aw = 5.0f * scale;
    float ah = 5.0f * scale;
    float r  = cfg.rounding;

    ImVec2 ref = (flags & (ToolTipFlags_FollowMouse | ToolTipFlags_FollowMouse_Fixed_Y)) ? item.mouse : item.center;

    ImVec2 target_dir = ref - center;
    float dist = ImLength(target_dir);
    ImVec2 dir = target_dir;
    if (dist > 0.0f) dir /= dist;
    
    if (anim && (flags & ToolTipFlags_Ease)) {
        anim->last_arrow_dir = ImLerp(anim->last_arrow_dir, dir, SMOOTH_LERP(anim->speedease, cfg.dt));
        dir = anim->last_arrow_dir;
        float len = ImLength(dir);
        if (len > 0.0f) dir /= len;
    }
    
    float k = ImClamp(dist / 200.0f, 0.0f, 1.0f);
    k = k * k * (3.0f - 2.0f * k);

    float aw_dyn = ImLerp(cfg.arrow_width * cfg.arrow_scale_min * scale, cfg.arrow_width * cfg.arrow_scale_max * scale, k);
    float ah_dyn = ImLerp(cfg.arrow_height * cfg.arrow_scale_min * scale, cfg.arrow_height * cfg.arrow_scale_max * scale, k);

    float dot = fabsf(dir.x * dir.y);
    if (dot > 0.45f) {
        aw_dyn *= 0.7f;
        ah_dyn *= 0.7f;
    }

    float hx = (pmax.x - pmin.x) * 0.5f;
    float hy = (pmax.y - pmin.y) * 0.5f;

    float tx = (fabsf(dir.x) > 1e-5f) ? hx / fabsf(dir.x) : FLT_MAX;
    float ty = (fabsf(dir.y) > 1e-5f) ? hy / fabsf(dir.y) : FLT_MAX;

    float t = ImMin(tx, ty);
    ImVec2 anchor = center + dir * t;

    float eps = ImMax(0.5f, scale * 0.5f);

    bool near_left   = fabs(anchor.x - pmin.x) < eps;
    bool near_right  = fabs(anchor.x - pmax.x) < eps;
    bool near_top    = fabs(anchor.y - pmin.y) < eps;
    bool near_bottom = fabs(anchor.y - pmax.y) < eps;

    bool is_corner =
        (near_top && near_left) ||
        (near_top && near_right) ||
        (near_bottom && near_left) ||
        (near_bottom && near_right);

    ImVec2 tangent, normal;
    ImVec2 corner_center;
    
    if (is_corner && r > 0.0f) {
        if (near_top && near_left)        corner_center = {pmin.x + r, pmin.y + r};
        else if (near_top && near_right)  corner_center = {pmax.x - r, pmin.y + r};
        else if (near_bottom && near_left)corner_center = {pmin.x + r, pmax.y - r};
        else                              corner_center = {pmax.x - r, pmax.y - r};

        ImVec2 v = anchor - corner_center;
        float l = ImLength(v);
        if (l > 0.0f) v /= l;
        anchor = corner_center + v * r;
        normal = v;
        tangent = ImVec2(-normal.y, normal.x);
    } else {
        enum Edge {Top, Bottom, Left, Right};
        Edge edge;

        if (near_top) edge = Top;
        else if (near_bottom) edge = Bottom;
        else if (near_left) edge = Left;
        else edge = Right;

        float min_x = pmin.x + r + aw_dyn;
        float max_x = pmax.x - r - aw_dyn;
        float min_y = pmin.y + r + aw_dyn;
        float max_y = pmax.y - r - aw_dyn;

        if (edge == Top || edge == Bottom)
            anchor.x = ImClamp(anchor.x, min_x, max_x);
        else
            anchor.y = ImClamp(anchor.y, min_y, max_y);

        if (edge == Top)        { tangent = ImVec2(1,0); normal = ImVec2(0,-1); }
        else if (edge == Bottom){ tangent = ImVec2(1,0); normal  = ImVec2(0,1); }
        else if (edge == Left)  { tangent = ImVec2(0,1); normal  = ImVec2(-1,0);}
        else                    { tangent = ImVec2(0,1); normal = ImVec2(1,0); }
    }

    ImVec2 arrow_p1 = anchor - tangent * aw_dyn;
    ImVec2 arrow_p2 = anchor + tangent * aw_dyn;
    ImVec2 arrow_p3 = anchor + normal  * ah_dyn;

    bool draw_arrow = !(flags & ToolTipFlags_NoArrow) && !cfg.skip_draw_arrow;
    ImDrawList* dl = cfg.draw_list;

    auto BuildPath = [&]() {
        dl->PathClear();
        const int ARC_SEG = 8;
        auto Arc = [&](ImVec2 c, float a0, float a1) { dl->PathArcTo(c, r, a0, a1, ARC_SEG); };

        if (draw_arrow && near_top && !is_corner) {
            float left  = pmin.x + r;
            float right = pmax.x - r;
            float x1 = ImClamp(arrow_p1.x, left, right);
            float x2 = ImClamp(arrow_p2.x, left, right);

            dl->PathLineTo(ImVec2(x1, pmin.y));
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(ImVec2(x2, pmin.y));
            dl->PathLineTo(ImVec2(pmax.x - r, pmin.y));
        } else {
            dl->PathLineTo(ImVec2(pmin.x + r, pmin.y)); 
            dl->PathLineTo(ImVec2(pmax.x - r, pmin.y)); 
        }

        Arc({pmax.x - r, pmin.y + r}, IM_PI*1.5f, IM_PI*2.0f);

        if (draw_arrow && near_right && !is_corner) {
            dl->PathLineTo(arrow_p1);
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(arrow_p2);
            dl->PathLineTo(ImVec2(pmax.x, pmax.y - r));
        } else {
            dl->PathLineTo(ImVec2(pmax.x, pmax.y - r));
        }

        Arc({pmax.x - r, pmax.y - r}, 0.0f, IM_PI*0.5f);

        if (draw_arrow && near_bottom && !is_corner) {
            dl->PathLineTo(ImVec2(arrow_p2.x, pmax.y));
            dl->PathLineTo(arrow_p2);
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(arrow_p1);
            dl->PathLineTo(ImVec2(pmin.x + r, pmax.y));
        } else {
            dl->PathLineTo(ImVec2(pmin.x + r, pmax.y));
        }

        Arc({pmin.x + r, pmax.y - r}, IM_PI*0.5f, IM_PI);

        if (draw_arrow && near_left && !is_corner) {
            dl->PathLineTo(ImVec2(pmin.x, arrow_p2.y));
            dl->PathLineTo(arrow_p2);
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(arrow_p1);
            dl->PathLineTo(ImVec2(pmin.x, pmin.y + r));
        } else {
            dl->PathLineTo(ImVec2(pmin.x, pmin.y + r));
        }

        Arc({pmin.x + r, pmin.y + r}, IM_PI, IM_PI*1.5f);

        if (draw_arrow && is_corner) {
            float angle = atan2f(anchor.y - corner_center.y, anchor.x - corner_center.x);
            float delta = cfg.arrow_corner_bias * (ah_dyn / r);
            float a0, a1;

            if (near_top && near_left)         { a0 = IM_PI; a1 = IM_PI*1.5f; }
            else if (near_top && near_right)   { a0 = IM_PI*1.5f; a1 = IM_PI*2.0f; }
            else if (near_bottom && near_right){ a0 = 0.0f; a1 = IM_PI*0.5f; }
            else                               { a0 = IM_PI*0.5f; a1 = IM_PI; }

            dl->PathArcTo(corner_center, r, a0, angle - delta);
            dl->PathLineTo(arrow_p1);
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(arrow_p2);
            dl->PathArcTo(corner_center, r, angle + delta, a1);
        }
    };
    BuildPath();
    if (!cfg.skip_draw_bg)
        dl->PathFillConvex(cfg.col_bg);

    if (!cfg.skip_draw_border && !(flags & ToolTipFlags_NoBorder)) {
        BuildPath();
        dl->PathStroke(cfg.col_border, ImDrawFlags_Closed, cfg.border_thickness);
    }    
}

static void RenderTooltipContent(TooltipData* td, const ImVec2& pmin, const ImVec2& pmax, const ImVec2& scaled) {
    auto& cfg = td->config;

    if (cfg.skip_draw_text || cfg.skip_draw) return;

    float pos_y = pmin.y + cfg.padding_content;
    
    auto DrawWrappedLines = [&](const std::vector<std::string>& lines, ImU32 color) {
        for (const auto& line : lines) {
            ImVec2 sz = cfg.font->CalcTextSizeA(cfg.fontsize, FLT_MAX, 0.0f, line.c_str());
            float draw = pmin.x + cfg.padding_content;
            if (cfg.align == ImGuiTooltip::Align_Center)
                draw += (scaled.x - sz.x) * 0.5f - cfg.padding_content;
            else if (cfg.align == ImGuiTooltip::Align_Right)
                draw += (scaled.x - sz.x);

            cfg.draw_list->AddText(cfg.font, cfg.fontsize, ImVec2(draw, pos_y), color, line.c_str());
            pos_y += sz.y + cfg.spacing_content;
        }
    };

    if (cfg.layout == ImGuiTooltip::Layout_Vertical) {
        if (cfg.show_image && cfg.image) {
            ImVec2 img_pos(pmin.x + (scaled.x - cfg.image_size.x) * 0.5f, pos_y);
            cfg.draw_list->AddImage(cfg.image, img_pos, img_pos + cfg.image_size);
            pos_y += cfg.image_size.y + cfg.spacing_content;
        }

        if (cfg.show_title) DrawWrappedLines(cfg.cached_title, cfg.col_title);
        if (cfg.show_text)  DrawWrappedLines(cfg.cached_text, cfg.col_text);
        if (cfg.show_extra) DrawWrappedLines(cfg.cached_extra, cfg.col_extra);
    } else if (cfg.layout == ImGuiTooltip::Layout_Horizontal) {
        float current_x = pmin.x + cfg.padding_content;

        if (cfg.show_image && cfg.image) {
            float img_y = pmin.y + (scaled.y - cfg.image_size.y) * 0.5f;
            ImVec2 img_pos(current_x, img_y);
            cfg.draw_list->AddImage(cfg.image, img_pos, img_pos + cfg.image_size);
            current_x += cfg.image_size.x + cfg.spacing_content;
        }

        float text_column_w = pmax.x - cfg.padding_content - current_x;
        float total_text_h = cfg.text_size.y;
        if (cfg.show_title) total_text_h += cfg.title_size.y + cfg.spacing_content;
        if (cfg.show_extra) total_text_h += cfg.extra_size.y + cfg.spacing_content;
        
        float start_text_y = pmin.y + (scaled.y - total_text_h) * 0.5f;

        auto DrawLinesHorizontal = [&](const std::vector<std::string>& lines, ImU32 color, float& y_offset) {
            for (const auto& line : lines) {
                ImVec2 sz = cfg.font->CalcTextSizeA(cfg.fontsize, FLT_MAX, 0.0f, line.c_str());
                float draw_x = current_x;

                if (cfg.align == ImGuiTooltip::Align_Center)
                    draw_x += (text_column_w - sz.x) * 0.5f;
                else if (cfg.align == ImGuiTooltip::Align_Right)
                    draw_x += (text_column_w - sz.x);

                cfg.draw_list->AddText(cfg.font, cfg.fontsize, ImVec2(draw_x, y_offset), color, line.c_str());
                y_offset += sz.y + 2.0f; 
            }
        };

        float run_y = start_text_y;

        if (cfg.show_title) {
            DrawLinesHorizontal(cfg.cached_title, cfg.col_title, run_y);
            run_y += cfg.spacing_content;
        }
        if (cfg.show_text)
            DrawLinesHorizontal(cfg.cached_text, cfg.col_text, run_y);

        if (cfg.show_extra) {
            run_y += cfg.spacing_content;
            DrawLinesHorizontal(cfg.cached_extra, cfg.col_extra, run_y);
        }
    }
}

void ToolTipEx(TooltipData* td, ToolTipFlags flags, TooltipCallback tooltip_cb) {
    if (tooltip_cb) tooltip_cb(Phase::AfterInit, Slot::None, td, NULL);
    
    auto& cfg  = td->config;
    auto  anim = td->anim;

    float alpha = 0.0f;
    UpdateTooltipState(td, flags, alpha);
    
    if (alpha > 0.01f) {
        ComputeTooltipLayout(td, flags, alpha, tooltip_cb);
        ComputeTooltipPosition(td, flags);

        ImVec2 draw_pos = td->out_pos, draw_size = td->out_size;
        
        if (flags & ToolTipFlags_Ease && anim) {
            anim->pos = ImLerp(anim->pos, td->out_pos, SMOOTH_LERP(anim->speedease, cfg.dt));
            anim->size = ImLerp(anim->size, td->out_size, SMOOTH_LERP(anim->speedease, cfg.dt));
            draw_pos = anim->pos; 
            draw_size = anim->size;
        }
        
        if (BeginTooltipWindow(td, draw_pos, draw_size)) {
            float scale = 1.0f;
            if (flags & ToolTipFlags_Scale) {
                scale = 0.85f + 0.15f * alpha;
            }

            ImVec2 center = draw_pos + draw_size * 0.5f;
            ImVec2 half = (draw_size * scale) * 0.5f;
            ImVec2 pmin = center - half;
            ImVec2 pmax = center + half;
            ImVec2 scaled = ImVec2((pmax.x - pmin.x), (pmax.y - pmin.y));

            if (tooltip_cb) tooltip_cb(Phase::Draw, Slot::Draw_layer0, td, cfg.draw_list);
            RenderTooltipBackground(td, flags, scale, center, pmin, pmax);

            if (tooltip_cb) tooltip_cb(Phase::Draw, Slot::Draw_layer1, td, cfg.draw_list);
            RenderTooltipContent(td, pmin, pmax, scaled);

            cfg.draw_list->PopClipRect();
            ImGui::End();
        }
    }
}

void CSImGui::ShowTooltipDelayed(const char* text, bool hovering, double delaySeconds) {
    ImGuiID base_id  = ImGui::GetID(text);
    ImGuiID delay_id = ImHashStr("delay", 0, base_id);
    ImGuiID alpha_id = ImHashStr("alpha", 0, base_id);

    bool shouldShow = SetDelayHover(hovering, delaySeconds, delay_id);
    float* pAlpha = ImGui::GetStateStorage()->GetFloatRef(alpha_id, 0.0f);

    float fadeSpeed = 12.0f;
    UpdateHoverAnim(*pAlpha, shouldShow, fadeSpeed);

    if (*pAlpha <= 0.001f) return;

    ImGui::PushStyleColor(ImGuiCol_PopupBg, GetColors(Col_PopupBg));
    ImGui::PushStyleColor(ImGuiCol_Border,  GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Text,    GetColors(Col_Text));

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, *pAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGui::SetNextWindowBgAlpha(*pAlpha);

    if (ImGui::BeginTooltip()) {
        ImGui::TextUnformatted(text);
        ImGui::EndTooltip();
    }

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(3);
}

bool CSImGui::ToolTip(const char* label, float delay, ToolTipFlags flags) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const char* label_end = strstr(label, "##");
    ImGuiID id = window->GetID((label_end) ? label_end : label);
    TooltipData* state = (TooltipData*)window->StateStorage.GetVoidPtr(id);
    if (!state) {
        state = (TooltipData*)IM_ALLOC(sizeof(TooltipData));
        IM_PLACEMENT_NEW(state) TooltipData();
        window->StateStorage.SetVoidPtr(id, state);
    }

    if ((flags & ToolTipFlags_Ease) && (flags & ToolTipFlags_Fade)) {
        ImGuiID anim_id = ImHashData(&id, sizeof(ImGuiID), 0xA449682);
        state->anim = (TooltipAnimState*)window->StateStorage.GetVoidPtr(anim_id);
        if (!state->anim) {
            state->anim = (TooltipAnimState*)IM_ALLOC(sizeof(TooltipAnimState));
            IM_PLACEMENT_NEW(state->anim) TooltipAnimState();
            window->StateStorage.SetVoidPtr(anim_id, state->anim);
        }
    }

    auto& cfg = state->config;
    auto& item = state->item;

    cfg.dt = ImGui::GetIO().DeltaTime;
    cfg.font = ImGui::GetFont();
    cfg.fontsize = ImGui::GetFontSize();
    item.mouse = ImGui::GetIO().MousePos;
    cfg.id = id;

    item.hovered = ImGui::IsItemHovered();
    item.active = ImGui::IsItemActive();
    item.pos = ImGui::GetItemRectMin();
    item.size = ImGui::GetItemRectSize();
    item.center = item.pos + item.size * 0.5f;
    cfg.max_width = 500.0f;
    cfg.max_height = 500.0f;
    cfg.title_max_lines = 5;

    if (item.hovered) item.hovered_time += cfg.dt;
    else item.hovered_time = 0;
    item.hover_delay = delay;

    if (label_end)
        cfg.title = TextUtils::Format("%.*s", (int)(label_end - label), label);
    else 
        cfg.title = label;

    cfg.show_title = !cfg.title.empty();
    cfg.show_text = false;

    item.cursor_size = ImGui::GetIO().MouseDrawCursor
        ? ImVec2(16, 16)
        : ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow
            ? ImVec2(16, 16)
            : ImVec2(20, 20);

    ToolTipFlags flags_ex = ToolTipFlags_FollowMouse | ToolTipFlags_AutoPosition | flags;
    ToolTipEx(state, flags_ex);

    return true;
}