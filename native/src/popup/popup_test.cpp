#include "popup_test.h"
#include "filter/audio_filter_manager.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

// Cấu trúc Logger Realtime để theo dõi sự kiện
struct LocalLogger {
    std::vector<std::string> Items;
    bool ScrollToBottom = false;

    void Log(const std::string& text) {
        Items.push_back(text);
        ScrollToBottom = true;
    }

    void Clear() { Items.clear(); }

    void Draw() {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "--- Nhật ký gọi lệnh (Realtime Logs) ---");
        ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        for (const auto& item : Items) {
            if (item.find("[FAIL]") != std::string::npos || item.find("Error") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", item.c_str());
            } else if (item.find("[PASS]") != std::string::npos || item.find("Thành công") != std::string::npos) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", item.c_str());
            } else {
                ImGui::TextUnformatted(item.c_str());
            }
        }
        ImGui::PopStyleVar();
        if (ScrollToBottom) { ImGui::SetScrollHereY(1.0f); ScrollToBottom = false; }
        ImGui::EndChild();
    }
};

static LocalLogger g_PopupLogger;

void ShowTestPopup(bool& closePopup_Test) {
    AudioFilterManager& manager = AudioFilterManager::Instance();

    // =========================================================================
    // PHẦN 1: BẢNG GIÁM SÁT TRẠNG THÁI CHI TIẾT (MANAGER STATE MONITOR)
    // =========================================================================
    if (ImGui::CollapsingHeader("1. Trạng thái chi tiết từ Manager (Core State)", ImGuiTreeNodeFlags_DefaultOpen)) {
        
        ImGui::Columns(3, "GlobalStateColumns", false);
        ImGui::Text("Kênh hiện tại: %s", manager.GetChannelMode().c_str()); 
        ImGui::NextColumn();
        ImGui::Text("Số Filter đang chạy: %d", manager.GetActiveFilterCount());
        ImGui::NextColumn();
        ImGui::Text("Track đang chọn: %s", manager.GetCurrentAudioTrack().c_str());
        ImGui::Columns(1);
        
        ImGui::Spacing();

        static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;
        
        if (ImGui::BeginTable("ManagerInternalTable", 4, flags)) {
            ImGui::TableSetupColumn("ID Bộ Lọc", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Tên Gốc (MPV)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Trạng Thái", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Tham Số Hiện Tại Trong Bộ Nhớ (Key: Value)");
            ImGui::TableHeadersRow();

            for (const auto& f : manager.GetFilters()) {
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", f.id.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(f.name.c_str());

                ImGui::TableSetColumnIndex(2);
                if (f.enabled) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● ENABLED");
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "○ DISABLED");
                }

                ImGui::TableSetColumnIndex(3);
                if (f.params.empty()) {
                    ImGui::TextDisabled("Không có tham số.");
                } else {
                    std::string param_dump = "";
                    for (const auto& [key, p] : f.params) {
                        std::ostringstream ss;
                        ss << key << ": [" << std::fixed << std::setprecision(2) << p.current << "]  ";
                        param_dump += ss.str();
                    }
                    ImGui::TextWrapped("%s", param_dump.c_str());
                }
            }
            ImGui::EndTable();
        }
    }

    // =========================================================================
    // PHẦN 2: BẢNG ĐIỀU KHIỂN CHI TIẾT TỰ ĐỘNG (DYNAMIC INTERACTIVE CONTROLS)
    // =========================================================================
    if (ImGui::CollapsingHeader("2. Cấu hình chi tiết bộ lọc (Audio Matrix)", ImGuiTreeNodeFlags_DefaultOpen)) {
        
        // Nút tính năng nhanh cho toàn bộ hệ thống
        if (ImGui::Button("Bật tất cả bộ lọc")) {
            manager.SetAllFiltersState(true);
            g_PopupLogger.Log("Kích hoạt đồng loạt tất cả bộ lọc.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Tắt tất cả bộ lọc")) {
            manager.SetAllFiltersState(false);
            g_PopupLogger.Log("Hủy kích hoạt đồng loạt tất cả bộ lọc.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Vòng lặp quét tự động qua từng filter có trong manager
        // Giờ đây bất cứ filter nào thêm ở Init() sẽ tự động xuất hiện ở đây
        for (const auto& filterRef : manager.GetFilters()) {
            // Lấy con trỏ non-const từ manager để có thể cập nhật trực tiếp qua UI
            auto* f = manager.FindFilter(filterRef.id);
            if (!f) continue;

            ImGui::PushID(f->id.c_str()); // Tránh trùng lặp ID ImGui cho các widget giống nhau

            bool isEnabled = f->enabled;
            // Hiển thị Checkbox bật/tắt chính cho Filter
            if (ImGui::Checkbox("##toggle", &isEnabled)) {
                manager.SetFilterEnabled(f->id, isEnabled);
                g_PopupLogger.Log("Thay đổi trạng thái " + f->id + " -> " + (isEnabled ? "ON" : "OFF"));
            }
            
            ImGui::SameLine();
            // Đổi màu chữ tiêu đề dựa trên trạng thái hoạt động của filter
            if (f->enabled) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[%s] - %s", f->id.c_str(), f->name.c_str());
            } else {
                ImGui::TextDisabled("[%s] - %s", f->id.c_str(), f->name.c_str());
            }

            // Nếu filter được bật, hoặc filter có tham số -> hiển thị vùng tinh chỉnh slider bên dưới
            if (f->enabled && !f->params.empty()) {
                ImGui::Indent(30.0f); // Đẩy lùi vào trong để tạo phân cấp UI trực quan
                
                // Quét qua map tham số của filter hiện tại
                for (auto& [key, param] : f->params) {
                    float val = param.current;
                    std::string label = key + "##" + f->id; // Gắn tag ID để tránh trùng label giữa các filter
                    
                    // Thiết lập định dạng hiển thị số thực tùy biến dựa theo dải tần hoặc âm lượng
                    const char* format = "%.2f";
                    if (param.max > 1000.0f) format = "%.0f Hz";
                    else if (key == "g" || key == "volume" || key == "threshold") format = "%.1f dB";

                    if (ImGui::SliderFloat(label.c_str(), &val, param.min, param.max, format)) {
                        // Gọi hàm UpdateParam tối ưu realtime xuống mpv
                        manager.UpdateParam(f->id, key, val);
                        g_PopupLogger.Log("Slider chỉnh " + f->id + " -> " + key + ": " + std::to_string(val));
                    }
                }
                
                // Nút Reset nhanh tham số của riêng bộ lọc này về mặc định
                if (ImGui::Button("Reset bộ lọc này")) {
                    manager.ResetFilter(f->id);
                    g_PopupLogger.Log("Đã đưa bộ lọc " + f->id + " về giá trị mặc định.");
                }
                ImGui::Unindent(30.0f);
                ImGui::Spacing();
            }
            
            ImGui::PopID();
            ImGui::Separator();
        }

        // --- Cấu hình Kênh & File I/O ---
        ImGui::Spacing();
        ImGui::Text("Cấu hình Luồng đầu ra (Channel Mode):");
        const char* modes[] = { "stereo", "mono", "surround" };
        for (int n = 0; n < 3; n++) {
            if (ImGui::Button(modes[n])) {
                manager.SetChannelMode(modes[n]);
                g_PopupLogger.Log(std::string("Thiết lập luồng đầu ra -> ") + modes[n]);
            }
            if (n < 2) ImGui::SameLine();
        }

        ImGui::Spacing();
        if (ImGui::Button("Lưu Cấu Hình (Save)")) {
            manager.SaveToFile();
            g_PopupLogger.Log("Cấu hình hiện tại đã được ghi xuống đĩa.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Tải Cấu Hình (Load)")) {
            manager.LoadFromFile();
            g_PopupLogger.Log("Đã nạp lại cấu hình từ file.");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // =========================================================================
    // PHẦN 3: LOG MONITOR
    // =========================================================================
    g_PopupLogger.Draw();

    ImGui::Spacing();
    if (ImGui::Button("Xóa Logs")) g_PopupLogger.Clear();
    ImGui::SameLine();
    if (ImGui::Button("Đóng")) closePopup_Test = true;
}

void OpenTestPopup(ReusablePopup& popup) {
    popup.Open("Audio Filter & Log Dashboard", [](bool& closePopup_Test) {
        ShowTestPopup(closePopup_Test);  
    });
}

void RenderTestPopup(ReusablePopup& popup){
    if(popup.IsOpen()) {
        popup.Render();
    }
}