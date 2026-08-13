#include "gui_buttons.h"
#include "../core/gui_theme.h"
#include <imgui_internal.h>
#include "utils.h"

static ImGuiID G_LastHoveredID = 0;

static void IconWrapperAdapter(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 col, void* user_data) {
    OldIconFn fn = reinterpret_cast<OldIconFn>(user_data);
    fn(dl, min, max, col);
}

const IconButtonStyle& CSImGui::GetDefaultIconButtonStyle() {
    static IconButtonStyle s;
    return s;
}

bool CSImGui::CustomIconButton(
    const char* str_id,
    void(*drawFn)(ImDrawList*, ImVec2, ImVec2, ImU32, void*),
    ImVec2 size,
    void* user_data,
    const IconButtonStyle& style
) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGuiID id = window->GetID(str_id);
    ImRect bb(pos, pos + size);

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (style.drawButtonBg) {
        ImU32 bg = held ? style.buttonBgActive : hovered ? style.buttonBgHovered : style.buttonBgColor;
        dl->AddRectFilled(bb.Min, bb.Max, bg, style.buttonRounding);
    }

    if (style.drawButtonBorder) {
        dl->AddRect(bb.Min, bb.Max, style.buttonBorderColor, style.buttonRounding, 0, style.buttonBorderThickness);
    }

    float padding = size.x * 0.2f;
    ImVec2 iconMin = pos + ImVec2(padding, padding);
    ImVec2 iconMax = pos + size - ImVec2(padding, padding);

    float scale = held ? style.iconActiveScale : hovered ? style.iconHoverScale : 1.0f;
    ImVec2 center = (iconMin + iconMax) * 0.5f;
    ImVec2 halfSize = (iconMax - iconMin) * 0.5f * scale;

    iconMin = center - halfSize;
    iconMax = center + halfSize;

    if (style.drawIconBg) {
        dl->AddRectFilled(iconMin, iconMax, style.iconBgColor, style.iconBgRounding);
    }

    if (style.drawIconBorder) {
        dl->AddRect(iconMin, iconMax, style.iconBorderColor, style.iconBgRounding, 0, style.iconBorderThickness);
    }

    ImU32 iconColor = ImGui::GetColorU32(held ? style.iconActive : hovered ? style.iconHovered : style.iconNormal);
    drawFn(dl, iconMin, iconMax, iconColor, user_data);

    return pressed;
}

bool CSImGui::CustomIconButton(const char* str_id, void(*drawFn)(ImDrawList*, ImVec2, ImVec2, ImU32, void*), ImVec2 size) {
    return CustomIconButton(str_id, drawFn, size, nullptr, GetDefaultIconButtonStyle());
}

bool CSImGui::CustomIconButton(const char* str_id, OldIconFn oldFn, ImVec2 size) {
    return CustomIconButton(str_id, IconWrapperAdapter, size, reinterpret_cast<void*>(oldFn), GetDefaultIconButtonStyle());
}

bool CSImGui::CustomIconButton(const char* str_id, void(*drawFn)(ImDrawList*, ImVec2, ImVec2, ImU32, void*), ImVec2 size, void* user_data) {
    return CustomIconButton(str_id, drawFn, size, user_data, GetDefaultIconButtonStyle());
}

bool CSImGui::ModernButton(const char* label, const ImVec2& size_arg, bool primary) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    
    ImVec2 padding = ImVec2(15, 8);
    ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + padding.x * 2.0f, label_size.y + padding.y * 2.0f);
    
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    float frameTime = g.IO.DeltaTime * 10.0f;
    float* pAnim = window->StateStorage.GetFloatRef(id, 0.0f);
    *pAnim = ImClamp(*pAnim + (hovered ? frameTime : -frameTime), 0.0f, 1.0f);

    ImVec4 col_base = primary ? GetColors(Col_Button) : ImVec4(0,0,0,0);
    ImVec4 col_hover = GetColors(Col_ButtonHovered);
    ImVec4 final_bg = ImLerp(col_base, col_hover, *pAnim);
    if (held) final_bg = GetColors(Col_ButtonActive);

    window->DrawList->AddRectFilled(bb.Min - ImVec2(2 * *pAnim, 1 * *pAnim), 
                                    bb.Max + ImVec2(2 * *pAnim, 1 * *pAnim), 
                                    ImGui::ColorConvertFloat4ToU32(final_bg), 6.0f);

    if (!primary) {
        window->DrawList->AddRect(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(GetColors(Col_Border)), 6.0f);
    }

    ImGui::RenderTextClipped(bb.Min, bb.Max, label, NULL, &label_size, ImVec2(0.5f, 0.5f));

    return pressed;
}

bool CSImGui::SecondaryButton(const char* label, const ImVec2& size) {
    return ModernButton(label, size, false);
}

bool CSImGui::ModernButtonEx(const char* label, const ImVec2& size_arg) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    
    ImVec2 padding = ImVec2(15, 8);
    ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + padding.x * 2.0f, label_size.y + padding.y * 2.0f);
    
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bb, id)) return false;
    
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    
    if (hovered) G_LastHoveredID = id;

    float frameTime = g.IO.DeltaTime * 10.0f;
    float* pAnim = window->StateStorage.GetFloatRef(id, 0.0f);
    *pAnim = ImClamp(*pAnim + (hovered ? frameTime : -frameTime), 0.0f, 1.0f);

    float influence = 0.0f;
    if (!hovered && G_LastHoveredID != 0) {
        influence = 0.2f;
    }
    
    ImVec4 final_bg = ImLerp(GetColors(Col_Button), GetColors(Col_ButtonHovered), *pAnim + influence);
    return pressed;
}

bool CSImGui::ModernSmallButton(const char* label) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    
    ImGui::PushStyleColor(ImGuiCol_Button,        GetColors(Col_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GetColors(Col_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  GetColors(Col_ButtonActive));
    ImGui::PushStyleColor(ImGuiCol_Text,          GetColors(Col_Text));

    bool pressed = ImGui::Button(label);

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    return pressed;
}

bool CSImGui::ModernArrowButton(const char* str_id, ImGuiDir dir, ImVec2 size) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    
    bool custom_size = (size.x != 0.0f || size.y != 0.0f);
    if (!custom_size) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    }

    ImGui::PushStyleColor(ImGuiCol_Button,        GetColors(Col_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GetColors(Col_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  GetColors(Col_ButtonActive));
    ImGui::PushStyleColor(ImGuiCol_Text,          GetColors(Col_Text));

    bool pressed = ImGui::Button(str_id, size);

    if (ImGui::IsItemVisible()) {
        ImVec2 pos_min = ImGui::GetItemRectMin();
        ImVec2 pos_max = ImGui::GetItemRectMax();
        
        float arrow_size = ImGui::GetFontSize();
        ImVec2 center = ImVec2(pos_min.x + (pos_max.x - pos_min.x) * 0.5f, 
                            pos_min.y + (pos_max.y - pos_min.y) * 0.5f);
        
        ImGui::RenderArrow(ImGui::GetWindowDrawList(), 
                        ImVec2(center.x - arrow_size * 0.45f, center.y - arrow_size * 0.45f), 
                        ImGui::GetColorU32(ImGuiCol_Text), 
                        dir);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);
    
    return pressed;
}

bool CSImGui::ModernCheckbox(const char* label, bool* v, CheckboxStyle style, const ImVec2& size_arg) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& imStyle = g.Style;
    const ImGuiID id = window->GetID(label);

    float square_sz = (size_arg.x > 0.0f) ? size_arg.x : ImGui::GetFrameHeight(); 
    ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    
    ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0 ? imStyle.ItemInnerSpacing.x + label_size.x : 0), square_sz));
    
    float padding_y = (size_arg.x > 0.0f) ? 0.0f : imStyle.FramePadding.y;
    ImGui::ItemSize(total_bb, padding_y);
    if (!ImGui::ItemAdd(total_bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    float* pT = window->StateStorage.GetFloatRef(id, *v ? 1.0f : 0.0f);
    *pT = ImClamp(*pT + (*v ? g.IO.DeltaTime * 12.0f : -g.IO.DeltaTime * 12.0f), 0.0f, 1.0f);

    float rounding = (style == CheckboxStyle::Circle) ? square_sz * 0.5f : 4.0f;
    ImU32 col_bg = ImGui::GetColorU32((held && hovered) ? GetColors(Col_FrameBgActive) : hovered ? GetColors(Col_FrameBgHovered) : GetColors(Col_FrameBg));
    
    window->DrawList->AddRectFilled(pos, pos + ImVec2(square_sz, square_sz), col_bg, rounding);
    window->DrawList->AddRect(pos, pos + ImVec2(square_sz, square_sz), ImGui::GetColorU32(ToCol32(GetColors(Col_Border)), 0.5f), rounding);

    if (*pT > 0.01f) {
        ImVec2 center = pos + ImVec2(square_sz * 0.5f, square_sz * 0.5f);
        ImU32 check_col = ImGui::GetColorU32(GetColors(Col_CheckMark));
        check_col = (check_col & 0x00FFFFFF) | ((uint32_t)(*pT * 255) << 24);

        if (style == CheckboxStyle::Circle) {
            window->DrawList->AddCircleFilled(center, (square_sz * 0.25f) * *pT, check_col);
        }
        else if (style == CheckboxStyle::Square) {
            float pad = square_sz * 0.25f * (1.0f - *pT + 1.0f);
            window->DrawList->AddRectFilled(pos + ImVec2(pad, pad), pos + ImVec2(square_sz - pad, square_sz - pad), check_col, 2.0f);
        }
        else if (style == CheckboxStyle::Tick) {
            float thickness = (square_sz < 18.0f) ? 1.5f : 2.0f; 
            float size = square_sz * 0.6f * *pT;
            float x0 = center.x - size * 0.5f, y0 = center.y;
            float x1 = center.x - size * 0.1f, y1 = center.y + size * 0.4f;
            float x2 = center.x + size * 0.5f, y2 = center.y - size * 0.4f;
            
            window->DrawList->PathLineTo(ImVec2(x0, y0));
            window->DrawList->PathLineTo(ImVec2(x1, y1));
            window->DrawList->PathLineTo(ImVec2(x2, y2));
            window->DrawList->PathStroke(check_col, 0, thickness);
        }
    }

    if (label_size.x > 0.0f) {
        float text_y_offset = (square_sz - g.FontSize) * 0.5f;
        ImVec2 text_pos = ImVec2(pos.x + square_sz + imStyle.ItemInnerSpacing.x, pos.y + text_y_offset);
        
        ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));
        ImGui::RenderText(text_pos, label);
        ImGui::PopStyleColor();
    }

    return pressed;
}

bool CSImGui::ModernToggle(const char* str_id, bool* v, bool enabled, float scale) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(str_id);

    float height = 18.0f * scale;
    float width = 36.0f * scale;
    float radius = height * 0.5f;

    ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = false;
    if (enabled) {
        pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) *v = !*v;
    }

    float frameTime = g.IO.DeltaTime * 12.0f;
    float* pT = window->StateStorage.GetFloatRef(id, *v ? 1.0f : 0.0f);
    
    if (enabled) {
        *pT = ImClamp(*pT + (*v ? frameTime : -frameTime), 0.0f, 1.0f);
    } else {
        *pT = *v ? 1.0f : 0.0f;
    }

    ImVec4 col_off = GetColors(Col_FrameBg);
    ImVec4 col_on  = GetColors(Col_CheckMark);
    ImVec4 current_col = ImLerp(col_off, col_on, *pT);
    
    float final_alpha = enabled ? 1.0f : 0.35f; 
    ImU32 bg_color = ImGui::ColorConvertFloat4ToU32(ImVec4(current_col.x, current_col.y, current_col.z, final_alpha));
    ImU32 knob_color = ImGui::ColorConvertFloat4ToU32(ImVec4(GetColors(Col_Text).x, GetColors(Col_Text).y, GetColors(Col_Text).z, final_alpha));

    window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_color, 10.0f);
    
    float knob_pos_x = bb.Min.x + radius + (*pT * (width - height));
    window->DrawList->AddCircleFilled(ImVec2(knob_pos_x, bb.Min.y + radius), radius - 2.0f, knob_color);

    return pressed;
}