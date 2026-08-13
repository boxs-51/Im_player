#pragma once

#include "../core/gui_types.h"
#include <vector>

namespace CSImGui {
    void InfoRow(const char *label, const char *fmt, ...);
    bool BeginInfoTable(const char *id, int column_count = 2, float first_col_width = 120.0f, ImGuiTableFlags extra_flags = 0);
    void EndInfoTable();
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
    bool BeginListTable(const char *id, const std::vector<TableCol> &cols, ImGuiTableFlags extra_flags = 0);
    void EndListTable();
    bool BeginListRow(float height = 28.0f);
    void EndListRow();
    bool IsRowClicked();
    bool ModernCollapsingHeader(const char *id, ImGuiTreeNodeFlags flags = 0);
    bool BeginModernPopup(const char *name, bool *open = NULL, ImGuiWindowFlags flags = 0);
    void EndModernPopup();
    bool ModernTreeNode(const char *id);
    void EndModernTreeNode();
    bool ModernSelectable(const char *label, bool selected, ImGuiSelectableFlags flags = 0, const ImVec2 &size_arg = ImVec2(0, 0), bool enabled = true);
    void ModernHeader(const char *title, float scale = 1.0f);
}