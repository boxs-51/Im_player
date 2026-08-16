#pragma once

#include "../core/gui_types.h"

#include "gui_status.h"
#include "gui_buttons.h"
#include "gui_slider.h"

#include <vector>

namespace CSImGui {

    bool BeginCard();
    void EndCard();
    bool BeginModernChild(const char *str_id, const ImVec2 &size = ImVec2(0, 0), bool border = false, ImGuiWindowFlags extra_flags = 0);
    void EndModernChild();
    bool BeginModernTabBar(const char *id, ImGuiTabBarFlags extra_flags = 0);
    void EndModernTabBar();
    bool ModernTabItem(const char *label, bool *p_open = NULL, ImGuiTabItemFlags flags = 0, ModernTabFlags m_flags = 0);
    void EndModernTabItem();
    void PushModernWindowStyle();
    void PopModernWindowStyle();

    bool ModernCollapsingHeader(const char *id, ImGuiTreeNodeFlags flags = 0);
    bool BeginModernPopup(const char *name, bool *open = NULL, ImGuiWindowFlags flags = 0);
    void EndModernPopup();
    bool ModernTreeNode(const char *id);
    void EndModernTreeNode();
    bool ModernSelectable(const char *label, bool selected, ImGuiSelectableFlags flags = 0, const ImVec2 &size_arg = ImVec2(0, 0), bool enabled = true);
    void ModernHeader(const char *title, float scale = 1.0f);

}