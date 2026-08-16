#include "gui_containers.h"
#include "../core/gui_theme.h"

#include <cstdarg>
#include <cstdio>
#include <imgui_internal.h>
#include "utils.h"




bool CSImGui::BeginCard() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, GetColors(Col_ChildBg)); 
    ImGui::PushStyleColor(ImGuiCol_Border,  GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Text,    GetColors(Col_Text));

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

    if (ImGui::BeginChild(ImGui::GetID("##card_inner"), ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
        return true;
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
    return false;
}

void CSImGui::EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
}

bool CSImGui::BeginModernChild(const char* str_id, const ImVec2& size, bool border, ImGuiWindowFlags extra_flags) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, GetColors(Col_ChildBg)); 
    ImGui::PushStyleColor(ImGuiCol_Border,  GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Text,    GetColors(Col_Text));

    if (ImGui::BeginChild(str_id, size, border, extra_flags | ImGuiWindowFlags_NoScrollbar)) return true;

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
    return false;
}

void CSImGui::EndModernChild() {
    ImGui::EndChild();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

bool CSImGui::BeginModernTabBar(const char* id, ImGuiTabBarFlags extra_flags) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(15.0f, 0.0f)); 
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 5.0f);
    
    ImGui::PushStyleColor(ImGuiCol_Text,          GetColors(Col_Text));
    ImGui::PushStyleColor(ImGuiCol_Tab,           GetColors(Col_Tab));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,    GetColors(Col_TabHovered));
    ImGui::PushStyleColor(ImGuiCol_TabActive,     GetColors(Col_TabActive));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused,  GetColors(Col_TabUnfocused));
    ImGui::PushStyleColor(ImGuiCol_Button,        GetColors(Col_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GetColors(Col_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  GetColors(Col_ButtonActive));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, GetColors(Col_TabUnfocusedActive));

    ImGuiTabBarFlags flags = ImGuiTabBarFlags_None;
    if (!(extra_flags & ImGuiTabBarFlags_FittingPolicyMask_)) {
        flags |= ImGuiTabBarFlags_FittingPolicyScroll;
    }

    flags |= extra_flags;
    if (ImGui::BeginTabBar(id, flags)) {
        return true;
    }

    ImGui::PopStyleColor(9);
    ImGui::PopStyleVar(3);
    return false;
}

void CSImGui::EndModernTabBar() {
    ImGui::EndTabBar();
    ImGui::PopStyleColor(9);
    ImGui::PopStyleVar(3);
}

bool CSImGui::ModernTabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags, ModernTabFlags m_flags) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(label);

    ImGui::PushStyleColor(ImGuiCol_Tab,                ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,         GetColors(Col_TabHovered));
    ImGui::PushStyleColor(ImGuiCol_TabActive,          ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused,       ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0,0,0,0));

    bool selected = ImGui::BeginTabItem(label, p_open, flags);

    if (ImGui::IsItemVisible()) {
        float* pT = ImGui::GetStateStorage()->GetFloatRef(id, 0.0f);
        if (!(m_flags & ModernTabFlags_NoAnimation)) {
            float target = ImGui::IsItemHovered() || selected ? 1.0f : 0.0f;
            *pT += (target - *pT) * g.IO.DeltaTime * 12.0f;
        } else {
            *pT = selected ? 1.0f : 0.0f;
        }

        if (*pT > 0.01f && !(m_flags & ModernTabFlags_NoIndicator)) {
            ImVec2 p_min = ImGui::GetItemRectMin();
            ImVec2 p_max = ImGui::GetItemRectMax();
            float full_width = p_max.x - p_min.x;
            float bar_width = (m_flags & ModernTabFlags_FullWidthBar) ? full_width : full_width * 0.8f;
            float offset = (full_width - bar_width) * 0.5f;

            ImVec2 b_min = ImVec2(p_min.x + offset, p_max.y - 2.0f);
            ImVec2 b_max = ImVec2(p_min.x + offset + (bar_width * *pT), p_max.y);

            ImVec4 col = selected ? GetColors(Col_CheckMark) : GetColors(Col_TextDisabled);
            col.w *= *pT;

            window->DrawList->AddRectFilled(b_min, b_max, ImGui::GetColorU32(col), 10.0f);
        }
    }
    if (!selected) {
        ImGui::PopStyleColor(5);
        return selected;
    }

    return selected;
}

void CSImGui::EndModernTabItem() {
    ImGui::EndTabItem();
    ImGui::PopStyleColor(5);
}

void CSImGui::PushModernWindowStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg,       GetColors(Col_WindowBg));
    ImGui::PushStyleColor(ImGuiCol_Text,           GetColors(Col_Text));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,        GetColors(Col_TitleBg));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  GetColors(Col_TitleBgActive));
    ImGui::PushStyleColor(ImGuiCol_Border,         GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Separator,      GetColors(Col_Separator));
}

void CSImGui::PopModernWindowStyle() {
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(5);
}



bool CSImGui::ModernCollapsingHeader(const char* id, ImGuiTreeNodeFlags flags) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

    ImGui::PushStyleColor(ImGuiCol_Header,        GetColors(Col_Header));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GetColors(Col_HeaderHovered));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  GetColors(Col_HeaderActive));
    ImGui::PushStyleColor(ImGuiCol_Text,          GetColors(Col_Text));

    bool res = ImGui::CollapsingHeader(id, flags);

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    return res;
}

bool CSImGui::BeginModernPopup(const char* name, bool* open, ImGuiWindowFlags flags) {
    ImGuiID id = ImGui::GetID(name);
    float* appear_anim = ImGui::GetStateStorage()->GetFloatRef(id + 10, 0.0f);
    
    if (ImGui::IsPopupOpen(name))
        *appear_anim = ImMin(*appear_anim + ImGui::GetIO().DeltaTime * 6.0f, 1.0f);
    else
        *appear_anim = 0.0f;

    CSImGui::PushModernWindowStyle();
    
    ImVec4 dim_col = ImVec4(0, 0, 0, 0.6f * (*appear_anim));
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, dim_col);

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool isOpen = ImGui::BeginPopupModal(name, open, flags | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    
    if (!isOpen) {
        ImGui::PopStyleColor();
        CSImGui::PopModernWindowStyle();
    }
    return isOpen;
}

void CSImGui::EndModernPopup() {
    ImGui::EndPopup();
    ImGui::PopStyleColor();
    CSImGui::PopModernWindowStyle();
}

bool CSImGui::ModernTreeNode(const char* id) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 6)); 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

    ImGui::PushStyleColor(ImGuiCol_Header,        GetColors(Col_Header)); 
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GetColors(Col_HeaderHovered)); 
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  GetColors(Col_HeaderActive)); 
    ImGui::PushStyleColor(ImGuiCol_Text,          GetColors(Col_Text)); 

    bool open = ImGui::TreeNode(id);
    
    if (!open) {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }
    return open;
}

void CSImGui::EndModernTreeNode() {
    ImGui::TreePop();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

bool CSImGui::ModernSelectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size_arg, bool enabled) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const ImGuiID id = window->GetID(label);
    ImGuiContext& g = *GImGui;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() + 8.0f);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bb, id)) return false;

    float* pTHover = window->StateStorage.GetFloatRef(id, 0.0f);
    float* pTSelect = window->StateStorage.GetFloatRef(id + 1, 0.0f);

    bool hovered, held;
    bool pressed = false;
    if (enabled) {
        pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
    }

    float frameTime = g.IO.DeltaTime * 12.0f;
    if (enabled) {
        *pTHover = ImClamp(*pTHover + (hovered ? frameTime : -frameTime), 0.0f, 1.0f);
        *pTSelect = ImClamp(*pTSelect + (selected ? frameTime : -frameTime), 0.0f, 1.0f);
    } else {
        *pTHover = 0.0f;
        *pTSelect = selected ? 1.0f : 0.0f;
    }

    if ((*pTHover > 0.0f || *pTSelect > 0.0f) && enabled) {
        if (hovered || selected) {
            ImVec4 bgColor = GetColors(Col_Header); 
            if (*pTHover > 0.0f) {
                bgColor = ImLerp(GetColors(Col_Header), GetColors(Col_HeaderHovered), *pTHover);
            }
            if (selected) bgColor = GetColors(Col_HeaderActive);

            window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(bgColor), 4.0f);
        }
    }

    if (enabled && *pTSelect > 0.0f) {
        float indicatorHeight = (bb.Max.y - bb.Min.y) * 0.6f;
        float centerY = (bb.Min.y + bb.Max.y) * 0.5f;
        
        ImVec2 p1(bb.Min.x + 2.0f, centerY - (indicatorHeight * 0.5f) * *pTSelect);
        ImVec2 p2(bb.Min.x + 4.5f, centerY + (indicatorHeight * 0.5f) * *pTSelect);
        
        window->DrawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(GetColors(Col_Button)), 10.0f);
    }

    if (label[0] != '#' || (label[1] != '\0' && label[1] != '#')) {
        float textOffsetX = 10.0f + (*pTHover * 4.0f);
        ImVec4 textColor = selected ? GetColors(Col_TextSelected) : GetColors(Col_Text);
        
        window->DrawList->AddText(ImVec2(pos.x + textOffsetX, pos.y + (size.y - g.FontSize) * 0.5f), 
                                ImGui::ColorConvertFloat4ToU32(textColor), label);
    }

    return pressed;
}

void CSImGui::ModernHeader(const char* title, float scale) {
    ImGui::Spacing();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float height = ImGui::GetFontSize() + (4.0f * scale);

    draw_list->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + 3.0f * scale, p.y + height), ToCol32(GetColors(Col_CheckMark)), 2.0f);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();

    ImVec4 sep_col = GetColors(Col_Separator);
    sep_col.w = 0.3f;
    ImGui::PushStyleColor(ImGuiCol_Separator, sep_col);
    ImGui::Separator();
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
}