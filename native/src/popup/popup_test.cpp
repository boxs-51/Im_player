#include "popup_test.h"
#include "filter/audio_filter_manager.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdlib> // Cần cho hàm rand() mô phỏng noise floor

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
        ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 180), true, ImGuiWindowFlags_HorizontalScrollbar);
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
    AudioContext ctx = manager.GetCurrentContext();
    
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

    // =========================================================================
    // PHẦN 3: ĐỒ THỊ VÀ BẢNG CHẨN ĐOÁN CHI TIẾT TỪ EBUR128 AUDIOCONTEXT (BẢN NÂNG CẤP)
    // =========================================================================
    if (ImGui::CollapsingHeader("3. Radar chẩn đoán Động học & Âm lượng nâng cao (EBU R128)", ImGuiTreeNodeFlags_DefaultOpen)) {
        
        // --- 1. QUẢN LÝ TRẠNG THÁI ĐỂ CHỐNG LẶP LOGS (STATE-MACHINE LOG ENGINE) ---
        enum class PeakState { SAFE, WARNING, CLIPPING };
        static PeakState last_peak_state = PeakState::SAFE;
        static bool last_hot_state = false;
        
        // Xác định trạng thái hiện tại dựa trên dữ liệu engine
        PeakState current_peak_state = PeakState::SAFE;
        if (ctx.true_peak_ch0 >= 1.0 || ctx.true_peak_ch1 >= 1.0) {
            current_peak_state = PeakState::CLIPPING;
        } else if (ctx.true_peak_ch0 > 0.95 || ctx.true_peak_ch1 > 0.95) {
            current_peak_state = PeakState::WARNING;
        }
        bool current_hot_state = (ctx.loudness_shortterm > -5.0);

        // CHỈ PHÁT LOG KHI CÓ SỰ CHUYỂN ĐỔI TRẠNG THÁI (EDGE TRIGGERING)
        if (current_peak_state != last_peak_state) {
            std::ostringstream ss;
            if (current_peak_state == PeakState::CLIPPING) {
                ss << "[CRITICAL] !!! HARD CLIPPING !!! Tín hiệu vượt ngưỡng vật lý (L: " 
                   << ctx.true_peak_ch0 << " | R: " << ctx.true_peak_ch1 << "). Âm thanh đang bị méo dạng!";
                g_PopupLogger.Log(ss.str());
            } 
            else if (current_peak_state == PeakState::WARNING) {
                ss << "[WARNING] Tín hiệu lọt vào vùng đỏ nguy hiểm (>0.95). Kiểm tra lại gain của nguồn phát.";
                g_PopupLogger.Log(ss.str());
            } 
            else if (current_peak_state == PeakState::SAFE && last_peak_state != PeakState::SAFE) {
                ss << "[SYSTEM] Tín hiệu đỉnh sóng đã hạ nhiệt và quay trở về vùng an toàn.";
                g_PopupLogger.Log(ss.str());
            }
            last_peak_state = current_peak_state; // Cập nhật trạng thái nền
        }

        if (current_hot_state != last_hot_state) {
            if (current_hot_state) {
                std::ostringstream ss;
                ss << "[AUDIO HOT] Tai người nghe có nguy cơ bị chói tai! Short-term Loudness quá cao: " << ctx.loudness_shortterm << " LUFS.";
                g_PopupLogger.Log(ss.str());
            }
            last_hot_state = current_hot_state;
        }

        // Luồng ghi log dữ liệu chi tiết định kỳ (Giữ nguyên hẹn giờ vì bản chất nó là log tiến trình)
        double currentTime = ImGui::GetTime();
        static double lastPeriodicLogTime = 0.0;
        const double PERIODIC_LOG_INTERVAL = 3.0; 
        static bool enablePeriodicDataLog = false;

        if (enablePeriodicDataLog && (currentTime - lastPeriodicLogTime > PERIODIC_LOG_INTERVAL)) {
            std::ostringstream data_ss;
            data_ss << "[DIAGNOSTICS] Snapshot (" << ctx.codec << " @" << ctx.bitrate_kbps << "kbps) "
                    << "| Integrated: " << std::fixed << std::setprecision(1) << ctx.loudness_integrated << " LUFS "
                    << "| LRA: " << ctx.loudness_range << " LU "
                    << "| Peak L/R: " << std::setprecision(3) << ctx.true_peak_ch0 << "/" << ctx.true_peak_ch1;
            g_PopupLogger.Log(data_ss.str());
            lastPeriodicLogTime = currentTime;
        }

        // --- THỐNG KÊ METADATA CƠ BẢN ---
        ImGui::Columns(3, "EbuMetaGrid", false);
        ImGui::Text("Codec Hiện Tại: %s", ctx.codec.empty() ? "N/A" : ctx.codec.c_str()); ImGui::NextColumn();
        ImGui::Text("Audio Bitrate: %.1f kbps", ctx.bitrate_kbps); ImGui::NextColumn();
        ImGui::Text("Chế độ Stream: %s", ctx.is_audio_only ? "Chỉ Audio" : "Video + Audio Stream");
        ImGui::Columns(1);
        ImGui::Separator();

        // --- HỆ THỐNG ĐỒ THỊ CUỘN TIME-SERIES CHO LOUDNESS ---
        const int HISTORY_SIZE = 128;
        static float momentary_history[HISTORY_SIZE] = { -70.0f };
        static float shortterm_history[HISTORY_SIZE] = { -70.0f };
        static int history_offset = 0;

        momentary_history[history_offset] = static_cast<float>(ctx.loudness_momentary);
        shortterm_history[history_offset] = static_cast<float>(ctx.loudness_shortterm);
        history_offset = (history_offset + 1) % HISTORY_SIZE;

        ImGui::Spacing();
        ImGui::Columns(2, "LoudnessGraphsGrid", true);

        ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "Độ to Tức thời (Momentary - 400ms)");
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 0.8f, 1.0f, 1.0f));
        ImGui::PlotLines("##M_LoudnessPlot", momentary_history, HISTORY_SIZE, history_offset, nullptr, -60.0f, 0.0f, ImVec2(0, 90));
        ImGui::PopStyleColor();
        ImGui::Text("Hiện tại: %.1f LUFS", ctx.loudness_momentary);

        ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Độ to Ngắn hạn (Short-term - 3s)");
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        ImGui::PlotLines("##S_LoudnessPlot", shortterm_history, HISTORY_SIZE, history_offset, nullptr, -60.0f, 0.0f, ImVec2(0, 90));
        ImGui::PopStyleColor();
        ImGui::Text("Hiện tại: %.1f LUFS", ctx.loudness_shortterm);

        ImGui::Columns(1);
        ImGui::Spacing();
        ImGui::Separator();

        // --- KHỐI GIÁM SÁT ĐỈNH SÓNG THỰC (LEVEL METERS) ---
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Đỉnh Sóng Thực Tuyến Tính (True Peak Level Meters):");
        
        char peak_text_buffer[64];
        float available_width = ImGui::GetContentRegionAvail().x;

        ImGui::Text("Ch 0 [L]:"); ImGui::SameLine(75.0f); 
        ImVec4 ch0_color = (ctx.true_peak_ch0 > 0.95) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.9f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ch0_color);
        sprintf(peak_text_buffer, "%.4f %s", ctx.true_peak_ch0, (ctx.true_peak_ch0 >= 1.0) ? "[CLIPPING!]" : "");
        ImGui::ProgressBar(static_cast<float>(ctx.true_peak_ch0), ImVec2(available_width, 16.0f), peak_text_buffer);
        ImGui::PopStyleColor();

        ImGui::Text("Ch 1 [R]:"); ImGui::SameLine(75.0f); 
        ImVec4 ch1_color = (ctx.true_peak_ch1 > 0.95) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.9f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ch1_color);
        sprintf(peak_text_buffer, "%.4f %s", ctx.true_peak_ch1, (ctx.true_peak_ch1 >= 1.0) ? "[CLIPPING!]" : "");
        ImGui::ProgressBar(static_cast<float>(ctx.true_peak_ch1), ImVec2(available_width, 16.0f), peak_text_buffer);
        ImGui::PopStyleColor();
        
        ImGui::Text("Đỉnh sóng tổng (Global True Peak): %.4f | Giới hạn an toàn: < 0.95", ctx.true_peak);

        ImGui::Spacing();
        ImGui::Separator();

        // --- 2. TÁI CẤU TRÚC: BIỂU ĐỒ ĐƯỜNG TUYẾN TÍNH LIÊN TỤC (LINEAR LINE & AREA CHART) ---
        const int WAVE_HISTORY_SIZE = 160; 
        static float history_current[WAVE_HISTORY_SIZE] = { -70.0f };
        static float history_avg[WAVE_HISTORY_SIZE] = { -70.0f };
        static int wave_offset = 0;

        // Lấy dữ liệu và tính toán Moving Average bằng bộ lọc thông thấp EMA (hệ số 0.05f)
        float current_loudness = static_cast<float>(ctx.loudness_momentary);
        static float running_avg = -70.0f;
        if (running_avg < -69.0f) running_avg = current_loudness; 
        running_avg = running_avg + 0.05f * (current_loudness - running_avg);

        // Nạp vào bộ đệm vòng (Ring Buffer)
        history_current[wave_offset] = current_loudness;
        history_avg[wave_offset] = running_avg;
        wave_offset = (wave_offset + 1) % WAVE_HISTORY_SIZE;

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.7f, 1.0f), "Đồ thị tuyến tính năng lượng (Đường nét: Tức thời | Vùng mờ: Trung bình tích lũy):");

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();           
        ImVec2 canvas_size = ImVec2(ImGui::GetContentRegionAvail().x, 90.0f); // Chiều cao nâng lên 90px để đồ thị tuyến tính biên độ rộng rõ ràng
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Vẽ Grid nền tối Studio
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(10, 12, 16, 255));
        draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(35, 40, 50, 255));

        // Vẽ các đường lưới ngang định chuẩn (Grid Lines) cho các mốc âm lượng quan trọng
        float lufs_markers[] = { -14.0f, -23.0f, -40.0f };
        for (float marker : lufs_markers) {
            float norm_m = (marker + 60.0f) / 60.0f;
            float y_m = canvas_pos.y + canvas_size.y - (norm_m * canvas_size.y * 0.90f) - (canvas_size.y * 0.05f);
            draw_list->AddLine(ImVec2(canvas_pos.x, y_m), ImVec2(canvas_pos.x + canvas_size.x, y_m), IM_COL32(50, 55, 65, 100), 1.0f);
        }

        float step_x = canvas_size.x / (WAVE_HISTORY_SIZE - 1);
        
        // Biến lưu tọa độ điểm trước đó để nối đường (Line Strip)
        ImVec2 prev_pt_cur, prev_pt_avg;

        for (int i = 0; i < WAVE_HISTORY_SIZE; i++) {
            int index = (wave_offset + i) % WAVE_HISTORY_SIZE;

            // 1. Chuẩn hóa giá trị Tức thời (Current)
            float lufs_cur = history_current[index];
            if (lufs_cur < -60.0f) lufs_cur = -60.0f;
            if (lufs_cur > 0.0f)  lufs_cur = 0.0f;
            float norm_cur = (lufs_cur + 60.0f) / 60.0f;

            // 2. Chuẩn hóa giá trị Trung bình (Average)
            float lufs_avg = history_avg[index];
            if (lufs_avg < -60.0f) lufs_avg = -60.0f;
            if (lufs_avg > 0.0f)  lufs_avg = 0.0f;
            float norm_avg = (lufs_avg + 60.0f) / 60.0f;

            // Tính toán tọa độ X và Y tuyến tính (Gốc tọa độ Y của ImGui tính từ đỉnh trên màn hình)
            // Chừa 5% padding trên dưới để đồ thị không bị chạm sát mép khung hình
            float x_pos = canvas_pos.x + (i * step_x);
            float graph_height_zone = canvas_size.y * 0.90f;
            float padding_y = canvas_size.y * 0.05f;

            float y_pos_cur = canvas_pos.y + canvas_size.y - (norm_cur * graph_height_zone) - padding_y;
            float y_pos_avg = canvas_pos.y + canvas_size.y - (norm_avg * graph_height_zone) - padding_y;
            float y_bottom  = canvas_pos.y + canvas_size.y;

            ImVec2 curr_pt_cur(x_pos, y_pos_cur);
            ImVec2 curr_pt_avg(x_pos, y_pos_avg);

            if (i > 0) {
                // ==========================================
                // LỚP 1: VẼ VÙNG ĐỔ BÓNG TRUNG BÌNH (AVERAGE AREA CHART)
                // ==========================================
                // Tạo một hình đa giác nhỏ nối từ đáy lên điểm cũ, qua điểm mới rồi hạ xuống đáy
                ImVec2 points[4] = {
                    ImVec2(prev_pt_avg.x, y_bottom - 1.0f),
                    prev_pt_avg,
                    curr_pt_avg,
                    ImVec2(curr_pt_avg.x, y_bottom - 1.0f)
                };
                
                ImU32 area_color = IM_COL32(0, 100, 160, 45); // Xanh dương mờ ảo làm nền
                if (history_avg[index] > -14.0f) {
                    area_color = IM_COL32(180, 80, 0, 45);   // Chuyển sang cam mờ nếu năng lượng trung bình cao
                }
                draw_list->AddConvexPolyFilled(points, 4, area_color);

                // Vẽ thêm một đường chỉ mảnh biên giới cho đường Trung bình mượt mà
                draw_list->AddLine(prev_pt_avg, curr_pt_avg, IM_COL32(0, 140, 200, 120), 1.5f);

                // ==========================================
                // LỚP 2: VẼ ĐƯỜNG TUYẾN TÍNH TỨC THỜI (CURRENT LINE GRAPH)
                // ==========================================
                ImU32 line_color;
                if (history_current[index] > -5.0f) {
                    line_color = IM_COL32(255, 30, 30, 255);   // Đỏ rực chói tai
                } else if (history_current[index] > -14.0f) {
                    line_color = IM_COL32(255, 150, 0, 255);  // Cam Modern Target
                } else {
                    line_color = IM_COL32(0, 255, 180, 255);  // Đường Cyan Neon mảnh mai, sắc nét
                }

                // Vẽ đường nối liền mạch giữa các frame dữ liệu
                draw_list->AddLine(prev_pt_cur, curr_pt_cur, line_color, 2.0f);
            }

            // Lưu lại điểm hiện tại làm điểm lùi cho frame kế tiếp
            prev_pt_cur = curr_pt_cur;
            prev_pt_avg = curr_pt_avg;
        }

        ImGui::Dummy(canvas_size); 
        ImGui::Spacing();
        ImGui::Separator();

        // =========================================================================
        // 4. TRẠM GIÁM SÁT BIÊN ĐỘ DAO ĐỘNG CỰC ĐẠI (LOUDNESS OSCILLATION MATRIX)
        // =========================================================================
        ImGui::TextColored(ImVec4(255.0f/255.0f, 150.0f/255.0f, 0.0f, 1.0f), "4. Trạm giám sát biên độ dao động cực đại (Loudness Oscillation Matrix):");

        // --- [NÂNG CẤP]: BIẾN PHÂN LY ĐỘ NHẠY ĐỒ HỌA ---
        // Cậu có thể chuyển biến này thành biến thành viên (member variable) của Class nếu muốn lưu cấu hình
        static float m_oscillationSensitivity = 1.0f; 
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SliderFloat("Độ nhạy phân ly", &m_oscillationSensitivity, 0.5f, 2.5f, "%.2fx");
        ImGui::SameLine(); ImGui::TextDisabled("(Tăng khi nhạc bị nén phẳng, giảm khi nhạc giật quá mạnh)");

        // --- TRÍCH XUẤT ĐỘ LỚN DAO ĐỘNG THỰC TẾ ---
        float loudness_delta = std::abs(static_cast<float>(ctx.loudness_momentary) - static_cast<float>(ctx.loudness_shortterm));
        
        // Khử nhiễu toán học khi bài hát rơi vào khoảng lặng
        if (ctx.loudness_momentary < -55.0f || ctx.loudness_shortterm < -55.0f) {
            loudness_delta = 0.0f;
        }

        // CHUẨN HÓA ĐỘ ĐỘNG THÍCH ỨNG (Áp dụng hệ số phân ly độ nhạy)
        // Nhân trực tiếp với m_oscillationSensitivity để khuếch đại hoặc thu nhỏ độ lệch Delta trước khi map vào dải [0.0f, 1.0f]
        float scaled_delta = loudness_delta * m_oscillationSensitivity;
        float raw_oscillation = std::min(1.0f, scaled_delta / 12.0f);

        // --- BỘ LỌC VẬT LÝ QUÁN TÍNH ĐỘNG NĂNG (ATTACK & DECAY PHYSICS ENGINE) ---
        static float oscillation_smooth = 0.0f;
        float dt = ImGui::GetIO().DeltaTime;
        
        if (raw_oscillation > oscillation_smooth) {
            oscillation_smooth = raw_oscillation; // Fast Attack
        } else {
            // Heavy Decay (Hệ số phục hồi 4.0f)
            oscillation_smooth -= 4.0f * dt * (oscillation_smooth - raw_oscillation);
            if (oscillation_smooth < 0.0f) oscillation_smooth = 0.0f;
        }

        // --- CẤU HÌNH KHÔNG GIAN VẼ BIỂU ĐỒ ---
        ImVec2 b_canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 b_canvas_size = ImVec2(ImGui::GetContentRegionAvail().x, 110.0f); 
        ImDrawList* b_draw_list = ImGui::GetWindowDrawList();

        // Vẽ nền tối Studio đặc trưng
        b_draw_list->AddRectFilled(b_canvas_pos, ImVec2(b_canvas_pos.x + b_canvas_size.x, b_canvas_pos.y + b_canvas_size.y), IM_COL32(8, 10, 14, 255));
        b_draw_list->AddRect(b_canvas_pos, ImVec2(b_canvas_pos.x + b_canvas_size.x, b_canvas_pos.y + b_canvas_size.y), IM_COL32(60, 45, 20, 255)); 

        // Xác định tâm hình học để dựng Lõi Động Năng
        ImVec2 center_node = ImVec2(b_canvas_pos.x + b_canvas_size.x / 2.0f, b_canvas_pos.y + b_canvas_size.y / 2.0f);
        
        // =========================================================================
        // LỚP 1: LÕI NĂNG LƯỢNG DAO ĐỘNG TRUNG TÂM (CENTRAL PULSING KINETIC CORE)
        // =========================================================================
        float base_radius = 18.0f;      
        float max_expansion = 20.0f;    
        float current_radius = base_radius + (oscillation_smooth * max_expansion);

        // Phát quang tỏa rạng (Kinetic Glow Aura)
        int aura_layers = 3;
        for (int i = 1; i <= aura_layers; i++) {
            float aura_radius = current_radius + (i * 5.0f * oscillation_smooth);
            int alpha = static_cast<int>((50 / i) * oscillation_smooth);
            b_draw_list->AddCircleFilled(center_node, aura_radius, IM_COL32(240, 120, 0, alpha), 32);
        }

        // Lõi năng lượng chính - Màu Cam Hổ Phách
        ImU32 core_color = IM_COL32(220, 100, 0, 255);
        if (oscillation_smooth > 0.80f) core_color = IM_COL32(255, 40, 0, 255); 
        
        b_draw_list->AddCircleFilled(center_node, current_radius, core_color, 36);
        b_draw_list->AddCircle(center_node, current_radius, IM_COL32(255, 230, 180, 200), 36, 1.5f); 

        // =========================================================================
        // LỚP 2: ĐỒ THỊ SÓNG DAO ĐỘNG ĐỐI XỨNG LAN TỎA (OSCILLATION WINGS)
        // =========================================================================
        const int OSCILLATION_HISTORY = 45;
        static float oscillation_history_array[OSCILLATION_HISTORY] = { 0.0f };
        static int oscillation_history_offset = 0;
        
        oscillation_history_array[oscillation_history_offset] = oscillation_smooth;
        oscillation_history_offset = (oscillation_history_offset + 1) % OSCILLATION_HISTORY;

        float wing_width = (b_canvas_size.x / 2.0f) - current_radius - 15.0f;
        float bar_step = wing_width / OSCILLATION_HISTORY;

        for (int i = 0; i < OSCILLATION_HISTORY; i++) {
            int index = (oscillation_history_offset - 1 - i + OSCILLATION_HISTORY) % OSCILLATION_HISTORY;
            float hist_val = oscillation_history_array[index];

            float bar_h = hist_val * (b_canvas_size.y * 0.42f);
            if (bar_h < 1.0f) bar_h = 1.0f; 

            int wave_alpha = static_cast<int>(std::max(15.0f, 255.0f * (1.0f - (static_cast<float>(i) / OSCILLATION_HISTORY))));
            ImU32 wing_color = IM_COL32(230, 140, 10, wave_alpha); 
            if (hist_val > 0.80f) wing_color = IM_COL32(255, 60, 40, wave_alpha); 

            // Cánh phải (Right Wing)
            float rx = center_node.x + current_radius + 10.0f + (i * bar_step);
            b_draw_list->AddRectFilled(ImVec2(rx, center_node.y - bar_h), ImVec2(rx + bar_step - 1.0f, center_node.y + bar_h), wing_color, 1.0f);

            // Cánh trái đối xứng (Left Wing)
            float lx = center_node.x - current_radius - 10.0f - (i * bar_step) - bar_step;
            b_draw_list->AddRectFilled(ImVec2(lx, center_node.y - bar_h), ImVec2(lx + bar_step - 1.0f, center_node.y + bar_h), wing_color, 1.0f);
        }


        ImGui::Dummy(b_canvas_size);
        ImGui::Spacing();
        ImGui::Separator();

        // --- KHỐI MA TRẬN PHÂN TÍCH DẢI ĐỘNG (LRA ADAPTIVE ANALYSIS) ---
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 1.0f, 1.0f), "Chỉ số tích lũy & Phân đoạn dải động (Loudness Range Matrix):");
        
        ImGui::Columns(3, "LraGrid", false);
        ImGui::Text("Độ to Tích lũy (Integrated):");
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "  %.1f LUFS", ctx.loudness_integrated);
        ImGui::NextColumn();
        
        ImGui::Text("Dải Động (LRA):");
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 1.0f, 1.0f), "  %.1f LU (Loudness Units)", ctx.loudness_range);
        ImGui::NextColumn();

        ImGui::Text("Cửa sổ Năng lượng (Low/High):");
        ImGui::TextDisabled("  Low: %.1f | High: %.1f", ctx.loudness_lra_low, ctx.loudness_lra_high);
        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Separator();

        // --- TIỆN ÍCH TRÍCH XUẤT VÀ KIỂM SOÁT LOG TẠI CHỖ ---
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "Bảng tương tác Nhật ký luồng tin:");
        ImGui::Checkbox("Tự động bơm Log dữ liệu chi tiết (Mỗi 3 giây)", &enablePeriodicDataLog);
        
        ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
        
        if (ImGui::Button("Xuất Báo Cáo Tức Thời (Dump Snapshot)")) {
            std::ostringstream ss;
            ss << "[MANUAL DUMP] ---- THÔNG SỐ ĐỘNG LỰC HỌC AUDIO ----\n"
               << " > Codec: " << (ctx.codec.empty() ? "N/A" : ctx.codec) << " | Bitrate: " << ctx.bitrate_kbps << " kbps\n"
               << " > Loudness Track -> M: " << ctx.loudness_momentary << " | S: " << ctx.loudness_shortterm << " | I: " << ctx.loudness_integrated << " LUFS\n"
               << " > Loudness Range (LRA): " << ctx.loudness_range << " LU (Vùng: " << ctx.loudness_lra_low << " -> " << ctx.loudness_lra_high << ")\n"
               << " > True Peak Max: " << ctx.true_peak << " (L: " << ctx.true_peak_ch0 << " / R: " << ctx.true_peak_ch1 << ")";
            g_PopupLogger.Log(ss.str());
        }
    }

    // =========================================================================
    // PHẦN 4: LOG MONITOR WINDOW
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