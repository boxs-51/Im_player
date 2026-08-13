#include "gui_inputs.h"
#include "../core/gui_theme.h"
#include "gui_containers.h"
#include <utils.h>
#include <unordered_map>
#include <algorithm>
#include <imgui_internal.h>

static int StringResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = (std::string*)data->UserData;
        str->resize(data->BufTextLen);
        data->Buf = str->data();
        data->BufSize = (int)str->capacity() + 1;
    }
    return 0;
}

static void RenderModernInputEffect(ImGuiID id, float* pFocusAnim) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    float target = ImGui::IsItemActive() ? 1.0f : 0.0f;
    *pFocusAnim += (target - *pFocusAnim) * ImGui::GetIO().DeltaTime * 10.0f;

    if (*pFocusAnim > 0.01f) {
        ImRect rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        rect.Expand(0.5f);
        ImVec4 accent_col = CSImGui::GetColors(Col_CheckMark);
        accent_col.w *= *pFocusAnim;
        window->DrawList->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(accent_col), 6.0f, 0, 1.5f);
    }
}

static bool DrawComboPopupBody(
    const char* label,
    std::string& current_value, 
    const std::vector<std::string>& display_options, 
    float custom_width, 
    int max_items_visible,
    std::function<bool(std::string&)> on_validate_confirm) 
{
    bool changed = false;

    float row_height = ImGui::GetTextLineHeightWithSpacing() + 12.0f;
    float display_count = (float)std::min((int)display_options.size(), max_items_visible);
    
    float child_height = display_options.empty() ? row_height : (display_count * row_height);
    std::string child_id = "##scrl_" + std::string(label);
    if (ImGui::BeginChild(child_id.c_str(), ImVec2(0, child_height), false, ImGuiWindowFlags_NoScrollbar)) {
        if (display_options.empty()) {
            ImGui::Indent(10);
            ImGui::TextDisabled("No results found");
            ImGui::Unindent(10);
        } else {
            for (const auto& opt : display_options) {
                bool is_selected = (current_value == opt);
                
                std::string truncated_opt = TextUtils::TruncateTextByPixels(opt.c_str(), custom_width - 25.0f);

                if (CSImGui::ModernSelectable(truncated_opt.c_str(), is_selected)) {
                    std::string validated_val = opt;
                    if (on_validate_confirm && on_validate_confirm(validated_val)) {
                        current_value = validated_val;
                    } else {
                        current_value = opt;
                    }
                    
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                
                if (ImGui::IsItemHovered() && truncated_opt != opt) {
                    ImGui::SetTooltip("%s", opt.c_str());
                }
                
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
        }
    }
    ImGui::EndChild();

    return changed;
}

bool CSImGui::ModernInputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags) {
    ImGuiID id = ImGui::GetCurrentWindow()->GetID(label);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  GetColors(Col_FrameBgActive));
    ImGui::PushStyleColor(ImGuiCol_Border,         GetColors(Col_Border)); 
    ImGui::PushStyleColor(ImGuiCol_Text,           GetColors(Col_Text));

    bool changed = ImGui::InputTextMultiline(label, buf, buf_size, size, flags);

    float* pFocusAnim = ImGui::GetStateStorage()->GetFloatRef(id + 1, 0.0f);
    RenderModernInputEffect(id, pFocusAnim);

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
    return changed;
}

bool CSImGui::ModernInputTextMultiline(const char* label, std::string& str, const ImVec2& size, ImGuiInputTextFlags flags) {
    ImGuiID id = ImGui::GetCurrentWindow()->GetID(label);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  GetColors(Col_FrameBgActive));
    ImGui::PushStyleColor(ImGuiCol_Border,         GetColors(Col_Border)); 
    ImGui::PushStyleColor(ImGuiCol_Text,           GetColors(Col_Text));

    flags |= ImGuiInputTextFlags_CallbackResize;

    if (str.empty()) {
        str.resize(1, '\0');
    }

    auto InputTextCallback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            std::string* local_str = (std::string*)data->UserData;
            IM_ASSERT(data->Buf == local_str->data());
            local_str->resize(data->BufTextLen);
            data->Buf = (char*)local_str->data();
        }
        return 0;
    };

    bool changed = ImGui::InputTextMultiline(
        label, 
        (char*)str.data(), 
        str.capacity() + 1, 
        size, 
        flags, 
        InputTextCallback, 
        (void*)&str
    );

    if (changed) {
        str.resize(strlen(str.data()));
    }

    float* pFocusAnim = ImGui::GetStateStorage()->GetFloatRef(id + 1, 0.0f);
    RenderModernInputEffect(id, pFocusAnim);

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
    return changed;
}

bool CSImGui::ModernInputText(const char* label, char* buf, size_t buf_size, float width, ImGuiInputTextFlags flags) {
    ImGuiID id = ImGui::GetID(label);
    
    float* pAnim = ImGui::GetStateStorage()->GetFloatRef(id + 500, 0.0f);
    bool is_active = ImGui::GetActiveID() == id;
    *pAnim += ((is_active ? 1.0f : 0.0f) - *pAnim) * ImGui::GetIO().DeltaTime * 12.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_Border, is_active ? GetColors(Col_CheckMark) : GetColors(Col_Border));

    ImGui::SetNextItemWidth(width);
    bool changed = ImGui::InputTextEx(label, "Type here...", buf, (int)buf_size, ImVec2(width, 0), flags);

    if (*pAnim > 0.01f) {
        ImRect rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        rect.Expand(1.5f * (*pAnim));
        ImVec4 glow_col = GetColors(Col_CheckMark);
        glow_col.w *= (*pAnim * 0.4f);
        ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(glow_col), 6.0f, 0, 1.5f);
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    return changed;
}

bool CSImGui::ModernInputText(const char* label, std::string& buffer, float width, ImGuiInputTextFlags flags) {
    ImGuiID id = ImGui::GetID(label);
    ImGuiStorage* store = ImGui::GetStateStorage();
    
    float* pAnim = store->GetFloatRef(id + 500, 0.0f);
    bool is_active = ImGui::GetActiveID() == id;
    *pAnim += ((is_active ? 1.0f : 0.0f) - *pAnim) * ImGui::GetIO().DeltaTime * 12.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    
    ImGui::PushStyleColor(ImGuiCol_FrameBg, GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_Border, is_active ? GetColors(Col_CheckMark) : GetColors(Col_Border));

    ImGui::SetNextItemWidth(width);
    
    flags |= ImGuiInputTextFlags_CallbackResize;
    bool changed = ImGui::InputTextEx(label, "Type to search...", (char*)buffer.data(), (int)buffer.capacity() + 1, ImVec2(width, 0), flags, StringResizeCallback, (void*)&buffer);

    if (*pAnim > 0.01f) {
        ImRect rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        rect.Expand(1.5f * (*pAnim));
        ImVec4 glow_col = GetColors(Col_CheckMark);
        glow_col.w *= (*pAnim * 0.4f);
        ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(glow_col), 6.0f, 0, 1.5f);
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    
    return changed;
}

bool CSImGui::ModernInputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8)); 
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        GetColors(Col_FrameBg)); 
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  GetColors(Col_FrameBgActive));
    ImGui::PushStyleColor(ImGuiCol_Border,         GetColors(Col_Border)); 
    ImGui::PushStyleColor(ImGuiCol_Text,           GetColors(Col_Text));

    bool changed = ImGui::InputText(label, buf, buf_size, flags);

    if (ImGui::IsItemActive()) {
        ImGui::GetWindowDrawList()->AddRect(
            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 
            ImColor(40, 110, 230, 255), 6.0f, 0, 1.5f
        );
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
    return changed;
}

bool CSImGui::ModernSearchCombo(const char* label,
    std::string& current_value,
    const std::vector<std::string>& options,
    float custom_width,
    int max_items_visible,
    std::function<bool(std::string&)> on_validate_confirm,
    std::function<std::string(const std::string&)> on_get_dynamic_opt)
{
    bool value_confirmed = false;
    ImGuiID popup_id = ImGui::GetID((std::string(label) + "_popup").c_str());
    ImGuiID anim_id = ImGui::GetID((std::string(label) + "_=searchanim").c_str());
    ImGuiID active_id_key = ImGui::GetID((std::string(label) + "_active").c_str());
    ImGuiID buffer_id_key = ImGui::GetID((std::string(label) + "_buffer").c_str());

    ImGuiStorage* store = ImGui::GetStateStorage();

    int* active_id = store->GetIntRef(active_id_key, 0);
    static std::unordered_map<ImGuiID, std::string> search_buffers;
    if (search_buffers.find(buffer_id_key) == search_buffers.end()) {
        search_buffers[buffer_id_key] = current_value;
    }
    std::string& search_buf = search_buffers[buffer_id_key];

    ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));
    ImGui::TextDisabled("%s", label);
    ImGui::PopStyleColor();

    ImGui::SetNextItemWidth(custom_width);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_Border,         GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Text,           GetColors(Col_Text));

    if (ModernInputText((std::string("##input_") + label).c_str(), search_buf, custom_width, ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (on_validate_confirm && on_validate_confirm(search_buf)) {
            current_value = search_buf;
            value_confirmed = true;
        }
        *active_id = 0;
        ImGui::CloseCurrentPopup();
    }

    float* pSearchAnim = ImGui::GetStateStorage()->GetFloatRef(anim_id, 0.0f);
    float search_target = (*active_id == (int)popup_id) ? 1.0f : 0.0f;
    *pSearchAnim += (search_target - *pSearchAnim) * ImGui::GetIO().DeltaTime * 12.0f;

    if (*pSearchAnim > 0.01f) {
        ImRect input_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        input_rect.Expand(1.0f * (*pSearchAnim));
        
        ImVec4 glow_col = GetColors(Col_CheckMark); 
        glow_col.w *= (*pSearchAnim * 0.4f);
        
        ImGui::GetWindowDrawList()->AddRect(input_rect.Min, input_rect.Max, 
            ImGui::GetColorU32(glow_col), 6.0f, 0, 1.5f);
    }
    if (ImGui::IsItemDeactivated()) {
        if (!value_confirmed) {
            search_buf = current_value;
        }
        *active_id = 0;
    }

    if (ImGui::IsItemActivated()) {
        *active_id = (int)popup_id;
        search_buf = current_value;
        ImGui::OpenPopup(popup_id);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 2));
    ImGui::SetNextWindowSizeConstraints(ImVec2(custom_width, 0), ImVec2(custom_width, FLT_MAX));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, GetColors(Col_PopupBg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));

    if (ImGui::BeginPopupEx(popup_id, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_ChildWindow)) {
        std::string filter = search_buf;
        std::string filter_lower = filter;
        std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);
        
        std::vector<std::string> filtered_options;

        std::string dynamic_val = "";
        if (on_get_dynamic_opt) {
            dynamic_val = on_get_dynamic_opt(filter);
            if (!dynamic_val.empty()) {
                filtered_options.push_back(dynamic_val);
            }
        }

        for (const auto& opt : options) {
            std::string o_lower = opt;
            std::transform(o_lower.begin(), o_lower.end(), o_lower.begin(), ::tolower);
            
            bool matches = filter_lower.empty() || o_lower.find(filter_lower) != std::string::npos;
            bool is_duplicate = (!dynamic_val.empty() && opt == dynamic_val);

            if (matches && !is_duplicate) {
                filtered_options.push_back(opt);
            }
        }

        if (DrawComboPopupBody(label, current_value, filtered_options, custom_width, max_items_visible, on_validate_confirm)) {
            value_confirmed = true;
            *active_id = 0; 
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    return value_confirmed;
}

bool CSImGui::NormalCombo(const char* label, std::string& current_item, const std::vector<std::string>& options, float custom_width, int max_items_visible) {
    bool changed = false;
    ImGuiID popup_id = ImGui::GetID((std::string(label) + "_popup").c_str());
    ImGuiID anim_id = ImGui::GetID((std::string(label) + "_anim").c_str());
    ImGuiID arrow_id = ImGui::GetID((std::string(label) + "_arrow").c_str());

    bool is_open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);

    ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));
    ImGui::TextDisabled("%s", label);
    ImGui::PopStyleColor();

    float* pAnim = ImGui::GetStateStorage()->GetFloatRef(anim_id, 0.0f);
    float* pArrowAnim = ImGui::GetStateStorage()->GetFloatRef(arrow_id, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    
    float frame_height = ImGui::GetFrameHeight();
    ImVec2 p_min = ImGui::GetCursorScreenPos();
    ImVec2 p_max = ImVec2(p_min.x + custom_width, p_min.y + frame_height);

    ImGui::PushID(label);
    ImGui::InvisibleButton("##btn", ImVec2(custom_width, frame_height));
    ImGui::PopID();
    ImRect rect(p_min, p_max);
    ImVec2 mouse = ImGui::GetIO().MousePos;

    bool is_hovered = rect.Contains(mouse);
    bool is_clicked = is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (is_clicked) {
        if (is_open)
            ImGui::CloseCurrentPopup();
        else
            ImGui::OpenPopupEx(popup_id);
    }
    is_open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);

    float target_anim = (is_hovered || is_open) ? 1.0f : 0.0f;
    *pAnim += (target_anim - *pAnim) * ImGui::GetIO().DeltaTime * 10.0f;
    
    float target_arrow = is_open ? 1.0f : 0.0f;
    *pArrowAnim += (target_arrow - *pArrowAnim) * ImGui::GetIO().DeltaTime * 12.0f;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 bg_col = ImGui::GetColorU32(is_open ? GetColors(Col_FrameBg) : (is_hovered ? GetColors(Col_FrameBgHovered) : GetColors(Col_FrameBg)));
    
    draw_list->AddRectFilled(p_min, p_max, bg_col, 6.0f);
    draw_list->AddRect(p_min, p_max, ImGui::GetColorU32(GetColors(Col_Border)), 6.0f, 0, 1.0f);

    if (*pAnim > 0.01f) {
        ImVec4 accent = GetColors(Col_CheckMark);
        accent.w *= (*pAnim * 0.4f);
        draw_list->AddRect(p_min, p_max, ImGui::GetColorU32(accent), 6.0f, 0, 1.5f);
    }

    float available_text_width = custom_width - 35.0f; 
    std::string truncated_display = TextUtils::TruncateTextByPixels(current_item.c_str(), available_text_width);
    draw_list->AddText(ImVec2(p_min.x + 10.0f, p_min.y + 8.0f), ImGui::GetColorU32(GetColors(Col_Text)), truncated_display.c_str());

    float arrow_size = 5.0f;
    ImVec2 arrow_center = ImVec2(p_max.x - 18.0f, p_min.y + frame_height * 0.5f);
    float angle = *pArrowAnim * IM_PI; 
    auto RotatePt = [](ImVec2 p, ImVec2 center, float ang) {
        float s = sin(ang), c = cos(ang);
        p.x -= center.x; p.y -= center.y;
        return ImVec2(p.x * c - p.y * s + center.x, p.x * s + p.y * c + center.y);
    };
    ImVec2 p1 = RotatePt(ImVec2(arrow_center.x - arrow_size, arrow_center.y - arrow_size * 0.4f), arrow_center, angle);
    ImVec2 p2 = RotatePt(ImVec2(arrow_center.x + arrow_size, arrow_center.y - arrow_size * 0.4f), arrow_center, angle);
    ImVec2 p3 = RotatePt(ImVec2(arrow_center.x, arrow_center.y + arrow_size * 0.6f), arrow_center, angle);
    draw_list->AddTriangleFilled(p1, p2, p3, ImGui::GetColorU32(is_open ? GetColors(Col_CheckMark) : GetColors(Col_Text)));

    ImGui::PopStyleVar(2);

    ImGui::SetNextWindowPos(ImVec2(p_min.x, p_max.y + 2));
    ImGui::SetNextWindowSizeConstraints(ImVec2(custom_width, 0), ImVec2(custom_width, FLT_MAX));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, GetColors(Col_PopupBg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));

    if (ImGui::BeginPopupEx(popup_id, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_ChildWindow)) {
        if (DrawComboPopupBody(label, current_item, options, custom_width, max_items_visible, nullptr)) {
            changed = true;
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    return changed;
}