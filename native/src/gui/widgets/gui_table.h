#pragma once

#include "gui_status.h"
#include "gui_buttons.h"
#include "gui_slider.h"
#include "../core/gui_types.h"

#include <vector>
namespace CSImGui {
    void InfoRow(const char *label, const char *fmt, ...);
    bool BeginInfoTable(const char *id, int column_count = 2, float first_col_width = 120.0f, ImGuiTableFlags extra_flags = 0);
    void EndInfoTable();

    void TableText(const char* text);
    void TableTextDisabled(const char* text);
    void TableStatus(const char* text , StatusType type = StatusType::None, BadgeFlags flags = BadgeFlags_Default);
    bool BeginListTable(const char *id, const std::vector<TableCol> &cols, ImGuiTableFlags extra_flags = 0);

    void TableNextCell(float item_height = 0.0f);


    bool TableCheckbox(const char* label, bool* v);
    bool TableButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    bool TableProgressBar(const char* label, float v, 
                                float height = 4.0f, const char* overlay = NULL, float width = -1.0f, ProgressBarFlags flags = ProgressBarFlags_Default);

    bool TableSliderFloat(const char* label, float* v, float v_min, float v_max, 
                        float height = 4.0f, float grab_radius = 8.0f, const char* format = "%.3f", float width = -1.0f, SliderFlags flags = SliderFlags_None);

    void TableCustomCell(float content_height, const std::function<void()>& render_func);

    void TableCustomCell(const std::function<void()>& render_func);

    void EndListTable();
    bool BeginListRow(int index, bool selected = false, bool* v = NULL, float height = 28.0f);
    bool BeginListRow(const char* row_id, bool selected = false, bool* v = NULL, float height = 28.0f);
    void EndListRow();
    bool IsRowClicked();

}