#include "ui_widgets.h"
#include "gui/gui.h"
#include "utils.h"
#include <string>

void UI_MenuItem(const char* label, const char* current_value, float scale, std::function<void()> on_click) {
    ImGui::PushID(label);
    
    float full_width = ImGui::GetContentRegionAvail().x;
    float item_height = 40.0f * scale;
    ImVec2 size = ImVec2(full_width, item_height); 

    if (CSImGui::ModernSelectable("##item", false, 0, size)) {
        on_click();
    }

    ImGuiID id = ImGui::GetItemID();
    float tHover = ImGui::GetStateStorage()->GetFloat(id, 0.0f);

    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float slide_offset = tHover * (4.0f * scale);
    float margin_left = (12.0f * scale) + slide_offset;
    float arrow_space = 35.0f * scale;
    float center_split = full_width * 0.55f;

    float max_label_w = center_split - margin_left - (5.0f * scale);
    std::string safe_label = TextUtils::TruncateToWidth(label, max_label_w);
    draw_list->AddText(ImVec2(p_min.x + margin_left, p_min.y + (item_height - ImGui::GetFontSize()) * 0.5f), 
                       ToCol32(CSImGui::GetColors(Col_Text)), safe_label.c_str());

    if (current_value && strlen(current_value) > 0) {
        float max_val_w = (full_width - center_split) - arrow_space - (5.0f * scale);
        std::string safe_value = TextUtils::TruncateToWidth(current_value, max_val_w);
        float val_text_width = ImGui::CalcTextSize(safe_value.c_str()).x;
        
        draw_list->AddText(ImVec2(p_max.x - val_text_width - arrow_space, p_min.y + (item_height - ImGui::GetFontSize()) * 0.5f), 
                           ToCol32(CSImGui::GetColors(Col_TextDisabled)), safe_value.c_str());
    }
    
    ImU32 arrow_col = tHover > 0.5f ? ToCol32(CSImGui::GetColors(Col_Button)) : IM_COL32(100, 100, 100, 255);
    draw_list->AddText(ImVec2(p_max.x - (20.0f * scale), p_min.y + (item_height - ImGui::GetFontSize()) * 0.5f), 
                       arrow_col, ">");
    
    ImGui::PopID();
}

void UI_Toggle(const char* label, bool* v, float scale, bool enabled, std::function<void(bool)> on_change) {
    ImGui::PushID(label);
    
    float full_width = ImGui::GetContentRegionAvail().x;
    float row_height = 35.0f * scale;
    float spacing_y = 2.0f * scale;
    
    ImVec2 size = ImVec2(full_width, row_height);
    ImVec2 p_min = ImGui::GetCursorScreenPos();

    if (CSImGui::ModernSelectable("##bg", false, 0, size, enabled) && enabled) {
        *v = !*v;
        if (on_change) on_change(*v);
    }

    ImGuiID id = ImGui::GetItemID();
    float tHover = ImGui::GetStateStorage()->GetFloat(id, 0.0f);
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float slide_offset = tHover * (4.0f * scale);
    float sw_w = 36.0f * scale;
    float sw_h = 18.0f * scale;
    
    float text_y_pos = p_min.y + (row_height - ImGui::GetFontSize()) * 0.5f;
    ImU32 text_col = enabled ? ToCol32(CSImGui::GetColors(Col_Text)) : ToCol32(CSImGui::GetColors(Col_TextDisabled));
    
    draw_list->AddText(ImVec2(p_min.x + (12.0f * scale) + slide_offset, text_y_pos), 
                       text_col, label);

    ImVec2 sw_pos = ImVec2(p_max.x - sw_w - 12.0f * scale, p_min.y + (row_height - sw_h) * 0.5f);
    
    ImVec2 backup_cursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(sw_pos);
    
    CSImGui::ModernToggle("##sw_internal", v, enabled, scale);
    
    ImGui::SetCursorScreenPos(backup_cursor); 
    ImGui::Dummy(ImVec2(0.0f, spacing_y));

    ImGui::PopID();
}

void UI_GroupHeader(const char* title, float scale) {
    ImGui::Spacing();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float height = ImGui::GetFontSize() + (4.0f * scale);

    draw_list->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + 3.0f * scale, p.y + height), ToCol32(CSImGui::GetColors(Col_CheckMark)), 2.0f);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_Text, CSImGui::GetColors(Col_Text));
    ImGui::Text(title);
    ImGui::PopStyleColor();

    ImVec4 sep_col = CSImGui::GetColors(Col_Separator);
    sep_col.w = 0.3f;
    ImGui::PushStyleColor(ImGuiCol_Separator, sep_col);
    ImGui::Separator();
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
}

void UI_SliderSpeed(const char* label, float* value, float min, float max, float scale, std::function<void(float)> on_change) {
    ImGui::PushID(label);
    
    float slider_height = 4.0f * scale;
    float grab_size = 7.0f * scale;
    float full_width = ImGui::GetContentRegionAvail().x;

    if (CSImGui::ModernSliderFloat(
            label,
            value,
            min,
            max,
            slider_height,
            grab_size,
            "%.2fx",
            full_width,
            SliderFlags_None
        )) 
    {
        if (on_change) on_change(*value);
    }
    
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0, 15.0f * scale));
}

void UI_SelectableItem(const char* label, bool is_active, float scale, std::function<void()> on_click) {
    ImGui::PushID(label);
    
    float full_width = ImGui::GetContentRegionAvail().x;
    float item_height = 35.0f * scale;
    ImVec2 size = ImVec2(full_width, item_height);

    if (CSImGui::ModernSelectable("##item_btn", is_active, 0, size)) {
        on_click();
    }

    ImGuiID id = ImGui::GetItemID();
    float tHover = ImGui::GetStateStorage()->GetFloat(id, 0.0f);
    float tSelect = ImGui::GetStateStorage()->GetFloat(id + 1, 0.0f);

    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    CSImGui::ToolTip(label, 1.5f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);

    float slide_offset = tHover * (4.0f * scale);
    float text_max_w = full_width - (45.0f * scale); 
    std::string display_text = TextUtils::TruncateTextByPixels(label, text_max_w);
    
    ImVec4 textColor = CSImGui::GetColors(Col_Text);
    if (tSelect > 0.0f) {
        textColor = ImLerp(CSImGui::GetColors(Col_Text), CSImGui::GetColors(Col_TextSelected), tSelect);
    }

    ImVec2 text_pos = ImVec2(p_min.x + (10.0f * scale) + slide_offset, p_min.y + (item_height - ImGui::GetFontSize()) * 0.5f);
    draw_list->AddText(text_pos, ToCol32(textColor), display_text.c_str());

    if (tSelect > 0.1f) {
        float check_size = (6.0f * scale) * tSelect;
        ImVec2 check_pos = ImVec2(p_max.x - 20 * scale, p_min.y + item_height * 0.5f);
        
        ImU32 check_col = ToCol32(CSImGui::GetColors(Col_CheckMark));
        check_col = (check_col & 0x00FFFFFF) | ((uint32_t)(tSelect * 255) << 24);
        
        draw_list->AddCircleFilled(check_pos, check_size * 0.5f, check_col);
    }

    ImGui::PopID();
}