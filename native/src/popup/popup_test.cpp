#include "popup_test.h"
#include "filter/audio_filter_manager.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>

// Cấu trúc Logger Realtime để theo dõi sự kiện nội bộ UI
struct LocalLogger {
    std::vector<std::string> Items;
    bool ScrollToBottom = false;
    
    void Log(const std::string& text) {
        Items.push_back(text);
        ScrollToBottom = true;
    }

    void Clear() { Items.clear(); }

    void Draw() {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "--- Nhật ký gọi lệnh & Hệ thống AI (Realtime Logs) ---");
        ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
        
        // 1. Render Logs cục bộ của giao diện UI tạo ra
        for (const auto& item : Items) {
            if (item.find("[SAFETY]") != std::string::npos || item.find("[FAIL]") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", item.c_str()); 
            } else if (item.find("[ADAPTIVE AI]") != std::string::npos) {
                ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "%s", item.c_str());  
            } else if (item.find("[BYPASS]") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "%s", item.c_str());  
            } else if (item.find("[PASS]") != std::string::npos || item.find("Thành công") != std::string::npos) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", item.c_str());  
            } else {
                ImGui::TextUnformatted(item.c_str()); 
            }
        }

        // 2. LẤY REAL-TIME LOGS TỪ LÕI MANAGER (Đã sửa lỗi đồng bộ, lấy trực tiếp trong frame vẽ)
        auto core_logs = AudioFilterManager::Instance().GetLogs();
        for (const auto& log : core_logs) {
            ImVec4 text_color;
            switch (log.level) {
                case LogLevel::Warning:   text_color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); break; 
                case LogLevel::Error:     text_color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break; 
                case LogLevel::AI_Action: text_color = ImVec4(0.2f, 0.8f, 1.0f, 1.0f); break; 
                case LogLevel::Info:
                default:                  text_color = ImVec4(0.9f, 0.9f, 0.9f, 1.0f); break; 
            }

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", log.timestamp.c_str());
            ImGui::SameLine();
            ImGui::TextColored(text_color, "%s", log.message.c_str());
        }
        
        if (ScrollToBottom) { 
            ImGui::SetScrollHereY(1.0f); 
            ScrollToBottom = false; 
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
};

static LocalLogger g_PopupLogger;

void ShowTestPopup(bool& closePopup_Test) {
    AudioFilterManager& manager = AudioFilterManager::Instance();

    // Trích xuất dữ liệu thô real-time từ lõi mpv thông qua manager để đưa lên màn hình debug
    int64_t sample_rate = 0;
    int64_t channel_count = 0;
    double bitrate_bps = 0.0;
    double current_volume = 0.0;
    mpv_handle* mpv = manager.GetMpvHandle(); 
    
    if (mpv) {
        mpv_get_property(mpv, "volume", MPV_FORMAT_DOUBLE, &current_volume);
        mpv_get_property(mpv, "audio-params/samplerate", MPV_FORMAT_INT64, &sample_rate);
        mpv_get_property(mpv, "audio-params/channel-count", MPV_FORMAT_INT64, &channel_count);
        mpv_get_property(mpv, "audio-bitrate", MPV_FORMAT_DOUBLE, &bitrate_bps);
    }

    // =========================================================================
    // PHẦN 1: BẢNG GIÁM SÁT TRẠNG THÁI CHI TIẾT + ĐỌC THÔNG SỐ AUDIO TRACK
    // =========================================================================
    if (ImGui::CollapsingHeader("1. Giám sát hệ thống Core & Phân tích Track Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(4, "TrackAnalysisColumns", false);
        ImGui::Text("Volume: %.1f dB", current_volume); ImGui::NextColumn();
        ImGui::Text("Sample Rate: %lld Hz", sample_rate); ImGui::NextColumn();
        ImGui::Text("Số Kênh: %lld Ch", channel_count); ImGui::NextColumn();
        ImGui::Text("Bitrate: %.1f kbps", bitrate_bps / 1000.0);
        ImGui::Columns(1);
        
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Columns(3, "GlobalStateColumns", false);
        ImGui::Text("Kênh Output: %s", manager.GetChannelMode().c_str()); ImGui::NextColumn();
        ImGui::Text("Filter Hoạt Động: %d", manager.GetActiveFilterCount()); ImGui::NextColumn();
        ImGui::Text("Quản Lý An Toàn: %s", manager.IsGlobalBypassEnabled() ? "BYPASS" : "ACTIVE");
        ImGui::Columns(1);
        
        ImGui::Spacing();

        // Bảng kết xuất cấu trúc ma trận filter nội bộ
        static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;
        if (ImGui::BeginTable("ManagerInternalTable", 5, flags)) {
            ImGui::TableSetupColumn("ID Node", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("Bộ Lọc FFmpeg", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Nhóm", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Trạng Thái", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Giá Trị Bộ Nhớ Realtime (Key: Value)");
            ImGui::TableHeadersRow();

            for (const auto& f : manager.GetFilters()) {
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", f.id.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(f.name.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("%s", f.group.empty() ? "None" : f.group.c_str());

                ImGui::TableSetColumnIndex(3);
                if (f.enabled) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● ENABLED");
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "○ DISABLED");
                }
                if (f.isBypassManagement) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[BYPASS]");
                }

                ImGui::TableSetColumnIndex(4);
                if (f.params.empty()) {
                    ImGui::TextDisabled("Không cấu hình tham số.");
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
    // PHẦN 2: BẢNG ĐIỀU KHIỂN CHẾ ĐỘ THÔNG MINH (ADAPTIVE ENGINE & BYPASS CONTROLS)
    // =========================================================================
    if (ImGui::CollapsingHeader("2. Trợ lý AI & Điều khiển Vượt tuyến (Adaptive Matrix)", ImGuiTreeNodeFlags_DefaultOpen)) {
        
        // --- KHỐI ĐIỀU KHIỂN BIÊN ĐỘ QUẢN LÝ (GLOBAL BYPASS) ---
        bool globalBypass = manager.IsGlobalBypassEnabled();
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 0.4f, 0.0f, 0.4f));
        if (ImGui::Checkbox("HỦY BỎ TOÀN BỘ HỆ THỐNG QUẢN LÝ (Global Bypass Security & Conflicts)", &globalBypass)) {
            manager.SetGlobalBypassMode(globalBypass);
            g_PopupLogger.Log(std::string("[BYPASS] Thay đổi trạng thái Global Bypass Manager -> ") + (globalBypass ? "ENABLED (Tự do ép xung)" : "DISABLED (Kích hoạt bảo vệ)"));
        }
        ImGui::PopStyleColor();
        
        ImGui::Spacing();

        // --- KHỐI QUẢN LÝ ĐỘC LẬP NGOẠI VI (ỔN ĐỊNH MẠCH VÀ TĂNG CƯỜNG ÂM LƯỢNG) ---
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Cơ cấu Bảo vệ & Kích âm Ngoại vi độc lập:");
        ImGui::Indent(10.0f);
        
        if (ImGui::Checkbox("Kích hoạt bộ Ổn định ngoại vi độc lập (f_out_compressor & f_out_limiter)", &manager.m_enableOuterStabilizer)) {
            manager.SetOuterStabilizerEnabled(manager.m_enableOuterStabilizer);
            g_PopupLogger.Log(std::string("[SYSTEM] Bộ bảo vệ màng loa ngoại vi -> ") + (manager.m_enableOuterStabilizer ? "BẬT" : "TẮT"));
        }
        
        ImGui::SameLine();
        ImGui::Spacing(); ImGui::SameLine();

        if (ImGui::Checkbox("Kích hoạt mạch Kích âm độc lập (f_vol_booster)", &manager.m_enableOuterBooster)) {
            manager.SetOuterBoosterEnabled(manager.m_enableOuterBooster);
            g_PopupLogger.Log(std::string("[SYSTEM] Mạch tăng cường âm Booster ngoại vi -> ") + (manager.m_enableOuterBooster ? "BẬT" : "TẮT"));
        }
        ImGui::Unindent(10.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- KHỐI ĐIỀU KHIỂN TRỢ LÝ THÔNG MINH (ADAPTIVE AI) ---
        bool autoMode = manager.IsAdaptiveMode();
        bool wasAutoMode = autoMode; 

        if (wasAutoMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
        }

        if (ImGui::Checkbox("KÍCH HOẠT ĐIỀU CHỈNH CHẤT ÂM TỰ ĐỘNG (Adaptive AI Engine)", &autoMode)) {
            manager.SetAdaptiveMode(autoMode, manager.GetCurrentPreset());
            g_PopupLogger.Log(std::string("[ADAPTIVE AI] Trạng thái bộ máy điều tiết tự động -> ") + (autoMode ? "BẬT" : "TẮT (Trả cấu hình mộc)"));
        }

        if (wasAutoMode) {
            ImGui::PopStyleColor(2);
        }

        if (autoMode) {
            ImGui::Indent(25.0f);
            const char* presetNames[] = { 
                "Cân bằng Studio (Flat)", 
                "Nhạc Trẻ / Pop Vocal", 
                "Heavy Rock / Metal", 
                "EDM / Dance Floor", 
                "Cổ điển / Classical",
                "Nhạc Mộc / Acoustic Guitar",
                "Chế độ Gaming (FPS Footsteps)",
                "Điện ảnh / Movie Cinema",
                "Siêu Trầm / Deep Bass"
            };
            int currentPresetIdx = static_cast<int>(manager.GetCurrentPreset());
            
            ImGui::SetNextItemWidth(280.0f);
            if (ImGui::Combo("Phong cách âm nhạc chủ đạo", &currentPresetIdx, presetNames, IM_ARRAYSIZE(presetNames))) {
                manager.SetCurrentPreset(static_cast<AudioPreset>(currentPresetIdx));
                g_PopupLogger.Log(std::string("[ADAPTIVE AI] Chuyển đổi Profile đáp tuyến tần số: ") + presetNames[currentPresetIdx]);
            }
            
            // Ghi Log tự động ra Monitor nếu có sự biến động lớn từ track
            static double last_logged_vol = 0.0;
            if (std::abs(current_volume - last_logged_vol) > 15.0) {
                if (current_volume < 30.0) g_PopupLogger.Log("[ADAPTIVE AI] Phát hiện âm lượng nhỏ. Đang bù gain dải tần hình chữ V (Loudness Equalization).");
                else if (current_volume > 80.0) g_PopupLogger.Log("[SAFETY] Phát hiện âm lượng vượt ngưỡng an toàn phần cứng. Tự động ép nén phẳng EQ để bảo vệ loa chống rách màng.");
                last_logged_vol = current_volume;
            }
            ImGui::Unindent(25.0f);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- DANH SÁCH CHI TIẾT TỪNG FILTER VÀ CÁC NÚT KHÓA TAY TỪNG PHẦN ---
        if (ImGui::Button("Bật toàn bộ hệ thống")) {
            manager.SetAllFiltersState(true);
            g_PopupLogger.Log("Người dùng ép bật thủ công toàn bộ các Node Filter.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Tắt toàn bộ hệ thống")) {
            manager.SetAllFiltersState(false);
            g_PopupLogger.Log("Người dùng ép tắt thủ công toàn bộ các Node Filter.");
        }

        ImGui::Spacing();

        for (const auto& filterRef : manager.GetFilters()) {
            auto* f = manager.FindFilter(filterRef.id);
            if (!f) continue;

            ImGui::PushID(f->id.c_str());

            // Checkbox điều khiển cơ bản từng node
            bool isEnabled = f->enabled;
            if (ImGui::Checkbox("##toggle", &isEnabled)) {
                manager.ToggleFilter(f->id, isEnabled);
                g_PopupLogger.Log("Thao tác thủ công: Thay đổi Node " + f->id + " -> " + (isEnabled ? "BẬT" : "TẮT"));
            }
            
            ImGui::SameLine();
            
            // Nút Quản lý Vượt tuyến (Bypass Node) - Cho phép giành lại quyền điều khiển từ AI
            if (f->isBypassManagement) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
                if (ImGui::Button("Mở Khóa AI")) {
                    manager.SetFilterBypassMode(f->id, false);
                    g_PopupLogger.Log("[BYPASS] Trả lại quyền quản lý Node " + f->id + " cho Hệ thống Tự động.");
                }
                ImGui::PopStyleColor();
            } else {
                if (ImGui::Button(" Khóa Tay ")) {
                    manager.SetFilterBypassMode(f->id, true);
                    g_PopupLogger.Log("[BYPASS] Tách biệt thành công Node " + f->id + " khỏi sự quản lý của AI.");
                }
            }

            ImGui::SameLine();
            
            // Đổi màu hiển thị tiêu đề node dựa theo trạng thái
            if (f->enabled) {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "[%s] Node %s (Group: %s)", f->id.c_str(), f->name.c_str(), f->group.c_str());
            } else {
                ImGui::TextDisabled("[%s] Node %s", f->id.c_str(), f->name.c_str());
            }

            // Vùng vẽ Slider tinh chỉnh cho từng tham số bên trong Filter
            if (!f->params.empty()) {
                ImGui::Indent(35.0f);
                
                // Nếu đang bật Adaptive AI điều khiển tự động và node này KHÔNG được chọn khóa tay -> Khóa các thanh kéo Slider
                bool lockWidgets = autoMode && !f->isBypassManagement;
                if (lockWidgets) ImGui::BeginDisabled();

                for (auto& [key, param] : f->params) {
                    float val = param.current;
                    std::string label = key + "##" + f->id;
                    
                    const char* format = "%.2f";
                    if (param.max > 1000.0f) format = "%.0f Hz";
                    else if (key == "g" || key == "volume" || key == "threshold") format = "%.1f dB";

                    if (ImGui::SliderFloat(label.c_str(), &val, param.min, param.max, format)) {
                        manager.UpdateParam(f->id, key, val);
                        g_PopupLogger.Log("Cập nhật tham số " + f->id + " -> " + key + ": " + std::to_string(val));
                    }
                }

                if (lockWidgets) ImGui::EndDisabled();

                if (!lockWidgets) {
                    ImGui::SameLine();
                    if (ImGui::Button("Reset Node")) {
                        manager.ResetFilter(f->id);
                        g_PopupLogger.Log("Đã khôi phục mặc định thông số cho riêng Node: " + f->id);
                    }
                }
                ImGui::Unindent(35.0f);
            }
            
            ImGui::PopID();
            ImGui::Separator();
        }

        // --- Cấu hình Kênh & File I/O ---
        ImGui::Spacing();
        ImGui::Text("Cấu hình Luồng đầu ra (Channel Matrix):");
        const char* modes[] = { "stereo", "mono", "surround" };
        for (int n = 0; n < 3; n++) {
            if (ImGui::Button(modes[n])) {
                manager.SetChannelMode(modes[n]);
                g_PopupLogger.Log(std::string("Thiết lập lại cấu hình ma trận kênh -> ") + modes[n]);
            }
            if (n < 2) ImGui::SameLine();
        }

        ImGui::Spacing();
        if (ImGui::Button("Lưu Cấu Hình (Save To Disk)")) {
            manager.SaveToFile();
            g_PopupLogger.Log("[PASS] Toàn bộ trạng thái phần cứng, Preset, cấu hình Bypass đã ghi xuống file JSON.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Tải Cấu Hình (Load From Disk)")) {
            manager.LoadFromFile();
            g_PopupLogger.Log("[PASS] Đã đồng bộ ngược trạng thái hoạt động từ file lưu trữ hệ thống.");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // =========================================================================
    // PHẦN 3: LOG MONITOR WINDOW
    // =========================================================================
    g_PopupLogger.Draw();

    ImGui::Spacing();
    if (ImGui::Button("Xóa bộ nhớ tạm Logs")) {
        g_PopupLogger.Clear();
        manager.ClearLogs(); // Xóa sạch cả log của Core Manager
    }
    ImGui::SameLine();
    if (ImGui::Button("Thoát Bảng Kiểm Thử")) closePopup_Test = true;
}

void OpenTestPopup(ReusablePopup& popup) {
    popup.Open("Audio Filter & Advanced Diagnostics Engine Dashboard", [](bool& closePopup_Test) {
        ShowTestPopup(closePopup_Test);  
    });
}

void RenderTestPopup(ReusablePopup& popup){
    if(popup.IsOpen()) {
        popup.Render();
    }
}