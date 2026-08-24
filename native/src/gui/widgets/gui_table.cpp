#include "gui_table.h"
#include "../core/gui_theme.h"
#include "gui_containers.h"

void CSImGui::InfoRow(const char* label, const char* fmt, ...) {
    if (!label) label = "";
    
    char buf[1024];
    bool isEmpty = true;

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        
        if (len > 0 && buf[0] != '\0') {
            isEmpty = false;
        }
    }

    ImGui::Spacing();
    ImGui::TableNextRow(ImGuiTableRowFlags_None, 24.0f);

    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", label);

    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();

    if (isEmpty) {
        ImGui::TextDisabled("None"); 
    } else {
        ImGui::TextUnformatted(buf);
    }
    ImGui::Spacing();
}

bool CSImGui::BeginInfoTable(const char* id, int column_count, float first_col_width, ImGuiTableFlags extra_flags) {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | 
                            ImGuiTableFlags_RowBg | 
                            ImGuiTableFlags_NoSavedSettings | 
                            ImGuiTableFlags_NoBordersInBody | 
                            extra_flags;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 4.0f));
    ImGui::Spacing();
    if (ImGui::BeginTable(id, column_count, flags)) {
        ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, first_col_width);
        for (int i = 1; i < column_count; i++) {
            ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);
        }

        ImGui::PushStyleColor(ImGuiCol_TableRowBg,    GetColors(Col_TableRowBg));
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, GetColors(Col_TableRowBgAlt)); 
        
        return true;
    }
    
    ImGui::PopStyleVar();
    return false;
}

void CSImGui::EndInfoTable() {
    ImGui::EndTable();
    ImGui::Spacing();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

bool CSImGui::BeginListTable(const char* id, const std::vector<TableCol>& cols, ImGuiTableFlags extra_flags) {
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | 
                            ImGuiTableFlags_SizingFixedFit | 
                            ImGuiTableFlags_NoSavedSettings | 
                            ImGuiTableFlags_BordersInnerV | 
                            extra_flags;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10, 8));

    if (ImGui::BeginTable(id, (int)cols.size(), flags)) {
        for (const auto& col : cols) {
            ImGuiTableColumnFlags c_flags = (col.width > 0.0f) ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch;
            ImGui::TableSetupColumn(col.name, c_flags, col.width);
        }

        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, GetColors(Col_TableHeaderBg));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  GetColors(Col_HeaderActive));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GetColors(Col_HeaderHovered));
        ImGui::PushStyleColor(ImGuiCol_Text,          GetColors(Col_Text));
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor(4);

        return true;
    }
    
    ImGui::PopStyleVar();
    return false;
}

void CSImGui::EndListTable() {
    ImGui::EndTable();
    ImGui::PopStyleVar();
}

void CSImGui::TableText(const char* text) {
    ImGui::TableNextColumn();
    if (text) ImGui::TextUnformatted(text);
}


void CSImGui::TableTextDisabled(const char* text) {
    ImGui::TableNextColumn();
    if (text) ImGui::TextDisabled("%s", text);
}

void CSImGui::TableStatus(const char* text, StatusType type, BadgeFlags flags) {
    ImGui::TableNextColumn();
    CSImGui::Badge(text, type, flags);
}

bool CSImGui::BeginListRow(const char* row_id, bool selected, bool* v, float height) {
    if (!row_id || row_id[0] == '\0') return false;

    ImGui::TableNextRow(ImGuiTableRowFlags_None, height);
    ImGui::TableNextColumn(); 
    
    ImGui::PushID(row_id);

    // Lưu lại vị trí bắt đầu vẽ của dòng
    ImVec2 start_pos = ImGui::GetCursorScreenPos();


    // ModernSelectable với SpanAllColumns và AllowItemOverlap
    ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns | 
                                 ImGuiSelectableFlags_AllowItemOverlap;

    bool is_clicked = ModernSelectable("##RowSelectable", selected, flags, ImVec2(0.0f, height));

    if (v != nullptr) {
        *v = is_clicked;
    }

    // Đưa con trỏ vẽ trở lại cột 1 để bắt đầu render UI từng cột
    ImGui::SetCursorScreenPos(start_pos);

    return true;
}

// Overload hỗ trợ dùng index tiện lợi hơn
bool CSImGui::BeginListRow(int index, bool selected, bool* v, float height) {
    char buf[32];
    snprintf(buf, sizeof(buf), "##row_%d", index);
    return BeginListRow(buf, selected, v, height);
}

void CSImGui::EndListRow() {
    ImGui::PopID();
}

bool CSImGui::IsRowClicked() {
    return ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped) && ImGui::IsMouseReleased(0);
}

void CSImGui::TableNextCell(float item_height) {
    ImGui::TableNextColumn();
    
    if (item_height > 0.0f) {
        ImGuiTable* table = ImGui::GetCurrentTable();
        if (table) {
            // Cách 1: Lấy chiều cao dòng tối thiểu đã thiết lập (ví dụ từ BeginListRow)
            float row_height = table->RowMinHeight;

            // Cách 2: Hoặc nếu RowMinHeight == 0, lấy chiều cao dòng thực tế vừa tính toán
            if (row_height <= 0.0f) {
                row_height = table->RowPosY2 - table->RowPosY1;
            }

            if (row_height > item_height) {
                float offsetY = (row_height - item_height) * 0.5f;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
            }
        }
    }
}


bool CSImGui::TableCheckbox(const char* label, bool* v) {
    TableNextCell(ImGui::GetFrameHeight());
    return CSImGui::ModernCheckbox(label, v);
}


bool CSImGui::TableButton(const char* label, const ImVec2& size) {
    float btn_h = (size.y > 0.0f) ? size.y : ImGui::GetFrameHeight();
    TableNextCell(btn_h);
    return CSImGui::ModernButton(label, size);
}



bool CSImGui::TableProgressBar(const char* label, float v, 
                                float height, const char* overlay, float width, ProgressBarFlags flags) 
{
    TableNextCell(height);
    return CSImGui::ModernProgressBar(label, v, height, overlay, width, flags);
}


bool CSImGui::TableSliderFloat(const char* label, float* v, float v_min, float v_max, 
                              float height, float grab_radius, const char* format, float width, SliderFlags flags) 
{
    TableNextCell(height * 2.5f); // Chiều cao tối đa tính cả grab radius
    return CSImGui::ModernSliderFloat(label, v, v_min, v_max, height, grab_radius, format, width, flags);
}

void CSImGui::TableCustomCell(float content_height, const std::function<void()>& render_func) {
    ImGui::TableNextColumn();

    if (!render_func) return;

    // Tự động căn giữa nội dung theo chiều dọc ô nếu truyền content_height > 0
    if (content_height > 0.0f) {
        ImGuiTable* table = ImGui::GetCurrentTable();
        if (table) {
            float row_height = table->RowMinHeight; 
            if (row_height > content_height) {
                float offset_y = (row_height - content_height) * 0.5f;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset_y);
            }
        }
    }

    // Nhóm các widget trong lambda lại để tránh tràn layout ngoài ý muốn
    ImGui::BeginGroup();
    render_func();
    ImGui::EndGroup();
}

void CSImGui::TableCustomCell(const std::function<void()>& render_func) {
    TableCustomCell(0.0f, render_func);
}