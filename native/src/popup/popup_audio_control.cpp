#include "popup_audio_control.h"
#include "audio_filter_manager.h"
#include "gui/gui.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

// =========================================================================
// HỆ THỐNG LOGGER REALTIME TÍCH HỢP TRONG POPUP
// =========================================================================
struct AudioPopupLogger {
    std::vector<std::string> Items;
    bool ScrollToBottom = false;

    void Log(const std::string& text) {
        Items.push_back(text);
        ScrollToBottom = true;
    }

    void Clear() { Items.clear(); }

    void Draw() {
        CSImGui::ModernHeader("--- Nhật ký hệ thống âm thanh (Realtime Logs) ---", 0.9f);
        CSImGui::BeginModernChild("AudioLogScrolling", ImVec2(0, 100), true, ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& item : Items) {
            if (item.find("[FAIL]") != std::string::npos || item.find("Error") != std::string::npos || item.find("Xung đột") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", item.c_str());
            } else if (item.find("[PASS]") != std::string::npos || item.find("Thành công") != std::string::npos || item.find("Preset") != std::string::npos) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", item.c_str());
            } else {
                ImGui::TextUnformatted(item.c_str());
            }
        }
        
        if (ScrollToBottom) { ImGui::SetScrollHereY(1.0f); ScrollToBottom = false; }
        CSImGui::EndModernChild();
    }
};

static AudioPopupLogger g_AudioLogger;

// =========================================================================
// UI COMPONENTS ĐỒNG BỘ QUA CSIMGUI VÀ AUDIOFILTERMANAGER
// =========================================================================

void DrawAudioTrackSelector(AudioFilterManager& audioMgr) {
    std::string current_track = audioMgr.GetCurrentAudioTrack();
    if (current_track.empty()) current_track = "Mặc định / Không tìm thấy";

    ImGui::TextDisabled("Kênh Audio Track:");
    
    std::vector<std::string> options;
    for (const auto& [id, display] : audioMgr.GetAudioTracks()) {
        options.push_back(display);
    }
    if (options.empty()) options.push_back("Mặc định / Không tìm thấy");

    // Thay thế Combo mặc định bằng NormalCombo sang trọng của CSImGui
    if (CSImGui::NormalCombo("##AudioTrackCombo", current_track, options, 240.0f)) {
        for (const auto& [id, display] : audioMgr.GetAudioTracks()) {
            if (display == current_track) {
                audioMgr.SelectAudioTrack(id);
                g_AudioLogger.Log("Chuyển Audio Track sang ID: " + id + " [" + display + "]");
                break;
            }
        }
    }
}

void DrawChannelDistributionControls(AudioFilterManager& audioMgr) {
    std::string current_mode = audioMgr.GetChannelMode();
    ImGui::TextDisabled("Hệ chuyển đổi Channel Out:");
    ImGui::Spacing();

    // Thiết kế hệ chuyển đổi kênh dạng danh sách Selectable hiện đại của CSImGui thay cho RadioButton
    if (CSImGui::ModernSelectable("Mono (Trộn đơn kênh)", current_mode == "mono", 0)) {
        audioMgr.SetChannelMode("mono");
        g_AudioLogger.Log("Thiết lập lại cấu hình kênh loa -> mono");
    }
    ImGui::Spacing();
    if (CSImGui::ModernSelectable("Stereo (Kênh đôi tiêu chuẩn)", current_mode == "stereo", 0)) {
        audioMgr.SetChannelMode("stereo");
        g_AudioLogger.Log("Thiết lập lại cấu hình kênh loa -> stereo");
    }
    ImGui::Spacing();
    if (CSImGui::ModernSelectable("Surround (Vòm giả lập 5.1)", current_mode == "surround", 0)) {
        audioMgr.SetChannelMode("surround");
        g_AudioLogger.Log("Thiết lập lại cấu hình kênh loa -> surround");
    }
}

void DrawAudioDashboardTab(AudioFilterManager& afMgr) {
    CSImGui::BeginCard();
    CSImGui::ModernHeader("Phím Tắt Tiện Ích", 1.0f);
    ImGui::Spacing();
    
    if (CSImGui::ModernButton("Bật Tất Cả Bộ Lọc", ImVec2(0.0f, 0.0f), true)) {
        afMgr.SetAllFiltersState(true);
        g_AudioLogger.Log("[PASS] Đã kích hoạt đồng loạt tất cả bộ lọc hiện có.");
    }
    ImGui::Spacing();
    if (CSImGui::SecondaryButton("Tắt Tất Cả Bộ Lọc", ImVec2(0.0f, 0.0f))) {
        afMgr.SetAllFiltersState(false);
        g_AudioLogger.Log("[PASS] Đã hủy kích hoạt toàn bộ các bộ lọc.");
    }
    ImGui::Spacing();
    if (CSImGui::SecondaryButton("Reset Về Mặc Định Core", ImVec2(0.0f, 0.0f))) {
        afMgr.ResetAllToDefaults();
        g_AudioLogger.Log("[Preset] Đã đưa các bộ lọc về cấu hình an toàn.");
    }
    CSImGui::EndCard();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    DrawAudioTrackSelector(afMgr);

    CSImGui::BeginCard();
    CSImGui::ModernHeader("Trạng Thái Thiết Bị Đầu Ra", 1.0f);
    ImGui::Spacing();
    if(CSImGui::BeginInfoTable("AudioOutputInfoTable", 2, 150.0f)) {
    CSImGui::InfoRow("Core Engine:", "libmpv Active");
    CSImGui::InfoRow("Audio Output:", "WASAPI (Exclusive)");
    CSImGui::InfoRow("Sample Rate:", "48000 Hz");
    CSImGui::InfoRow("Latency Control:", "Low Overhead");
    CSImGui::EndInfoTable();
    }
    CSImGui::EndCard();
}

void DrawEqualizerAndChannelTab(AudioFilterManager& afMgr) {
    CSImGui::ModernHeader("Định Chính Equalizer (5-Band Mặc Định)", 1.0f);
    ImGui::Spacing();

    // Lấy band đầu tiên làm mốc đại diện để kiểm tra trạng thái Bật/Tắt của cả hệ thống EQ
    auto* eq_master = afMgr.FindFilter("eq_band0");
    bool eq_enabled = eq_master ? eq_master->enabled : false;

    if (CSImGui::ModernCheckbox("Kích Hoạt Equalizer Engine", &eq_enabled, CheckboxStyle::Tick)) {
        // Bật hoặc tắt đồng loạt cả 5 dải tần
        afMgr.ToggleFilter("eq_band0", eq_enabled);
        afMgr.ToggleFilter("eq_band1", eq_enabled);
        afMgr.ToggleFilter("eq_band2", eq_enabled);
        afMgr.ToggleFilter("eq_band3", eq_enabled);
        afMgr.ToggleFilter("eq_band4", eq_enabled);
        g_AudioLogger.Log(std::string("Thay đổi trạng thái Toàn bộ Hệ EQ -> ") + (eq_enabled ? "BẬT" : "TẮT"));
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!eq_enabled) ImGui::BeginDisabled();

    const char* bands[5] = { "Sub-Bass (60 Hz)", "Bass/Low (250 Hz)", "Midrange (1 kHz)", "Upper-Mid (4 kHz)", "Treble/High (16 kHz)" };
    const char* filter_ids[5] = { "eq_band0", "eq_band1", "eq_band2", "eq_band3", "eq_band4" };

    if (ImGui::BeginTable("EQ_Grid_Core", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Slider", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthFixed, 65.0f);

        for (int i = 0; i < 5; i++) {
            auto* f_band = afMgr.FindFilter(filter_ids[i]);
            if (!f_band) continue;

            ImGui::TableNextRow(ImGuiTableRowFlags_None, 24.0f);
            
            // Cột 1: Label dải tần
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", bands[i]);

            // Cột 2: Slider điều chỉnh gain 'g'
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(i);
            
            float current_gain = f_band->params.count("g") ? f_band->params.at("g").current : 0.0f;
            float available_width = ImGui::GetContentRegionAvail().x;

            if (CSImGui::ModernSliderFloat("##slider", &current_gain, -20.0f, 20.0f, 4.0f, 6.0f, "%.1f dB", available_width)) {
                afMgr.UpdateParam(filter_ids[i], "g", current_gain);
                g_AudioLogger.Log("Cập nhật " + std::string(bands[i]) + " -> " + std::to_string(current_gain) + " dB");
            }

            // Cột 3: Ô nhập số trực tiếp
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(65.0f);
            if (ImGui::InputFloat("##input", &current_gain, 0.0f, 0.0f, "%.1f")) {
                current_gain = std::clamp(current_gain, -20.0f, 20.0f);
                afMgr.UpdateParam(filter_ids[i], "g", current_gain);
                g_AudioLogger.Log("Nhập thủ công " + std::string(bands[i]) + " -> " + std::to_string(current_gain) + " dB");
            }
            
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!eq_enabled) ImGui::EndDisabled();

    // Khối cấu hình kênh loa giữ nguyên phía dưới
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    CSImGui::ModernHeader("Cấu Hình Kênh Loa (Output Channels)", 1.0f);
    ImGui::Spacing();
    CSImGui::BeginCard();
    DrawChannelDistributionControls(afMgr);
    CSImGui::EndCard();
}

void DrawDynamicMatrixTab(AudioFilterManager& afMgr) {
    CSImGui::ModernHeader("Bộ Lọc Hệ Thống (Audio Filter Dynamic Matrix)", 1.0f);
    ImGui::Spacing();

    // Vòng lặp tự động sinh giao diện từ Core cấu hình của AudioFilterManager
    for (const auto& filterRef : afMgr.GetFilters()) {
        // LOẠI TRỪ: Không hiển thị f_eq ở đây vì đã được đưa ra ngoài tab mặc định chuyên nghiệp
        if (filterRef.id == "f_eq") continue;

        auto* f = afMgr.FindFilter(filterRef.id);
        if (!f) continue;

        ImGui::PushID(f->id.c_str());

        std::string header_title = "[" + f->id + "] " + f->name;
        if (CSImGui::ModernCollapsingHeader(header_title.c_str())) {
            CSImGui::BeginCard();
            
            bool isEnabled = f->enabled;
            if (CSImGui::ModernCheckbox("Bật bộ lọc hiệu ứng", &isEnabled, CheckboxStyle::Tick)) {
                afMgr.ToggleFilter(f->id, isEnabled);
                g_AudioLogger.Log("Thay đổi trạng thái bộ lọc " + f->id + " -> " + (isEnabled ? "BẬT" : "TẮT"));
            }
            
            ImGui::SameLine(ImGui::GetWindowWidth() - 160.0f);
            if (CSImGui::ModernSmallButton("Reset bộ lọc")) {
                afMgr.ResetFilter(f->id);
                g_AudioLogger.Log("Đưa bộ lọc " + f->id + " về cấu hình mặc định.");
            }

            // Sinh Slider động dựa trên đặc tính tham số
            if (f->enabled && !f->params.empty()) {
                ImGui::Spacing();
                ImGui::Indent(15.0f);
                for (auto& [key, param] : f->params) {
                    float val = param.current;
                    ImGui::Text("%s:", key.c_str());
                    
                    // Thiết lập đơn vị hiển thị thông minh (Hz / dB) dựa trên tên key tham số
                    const char* fmt = "%.2f";
                    if (param.max > 500.0f) fmt = "%.0f Hz";
                    else if (key == "g" || key == "volume" || key == "threshold") fmt = "%.1f dB";

                    ImGui::PushID(key.c_str());
                    // Sử dụng Slider nâng cao của bạn có tích hợp hiệu ứng Animation và Tooltip
                    if (CSImGui::ModernSliderFloat("##dyn_slider", &val, param.min, param.max, 4.0f, 6.0f, fmt, -1.0f, SliderFlags_Default)) {
                        afMgr.UpdateParam(f->id, key, val);
                        g_AudioLogger.Log("Cập nhật realtime " + f->id + " -> " + key + ": " + std::to_string(val));
                    }
                    ImGui::PopID();
                }
                ImGui::Unindent(15.0f);
            }
            CSImGui::EndCard();
            ImGui::Spacing();
        }
        ImGui::PopID();
    }
}

// =========================================================================
// HÀM ĐIỀU HƯỚNG CORE POPUP (ENTRY POINT)
// =========================================================================

void ShowAudioControlPopup(bool& closePopup_AudioControl) {
    auto& afMgr = AudioFilterManager::Instance();
    
    // Áp dụng kiến trúc Tab-Bar hiện đại thông qua CSImGui
    if (CSImGui::BeginModernTabBar("##AudioSystemMainTabs")) {
        
        // Tab 1: Mặc định định chính EQ và Hệ chuyển đổi kênh loa đầu ra (Yêu cầu chính)
        if (CSImGui::ModernTabItem("Định Chính EQ & Kênh Out")) {
            ImGui::Spacing();
            DrawEqualizerAndChannelTab(afMgr);
            CSImGui::EndModernTabItem();
        }

        // Tab 2: Quản lý tổng quan Audio Dashboard
        if (CSImGui::ModernTabItem("Bảng Tổng Quan (Audio Dashboard)")) {
            ImGui::Spacing();
            DrawAudioDashboardTab(afMgr);
            CSImGui::EndModernTabItem();
        }

        // Tab 3: Matrix động quét tự động cấu hình JSON còn lại
        if (CSImGui::ModernTabItem("Bộ Lọc Hệ Thống Matrix")) {
            ImGui::Spacing();
            DrawDynamicMatrixTab(afMgr);
            CSImGui::EndModernTabItem();
        }

        CSImGui::EndModernTabBar();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Khối hiển thị màn hình Console Nhật ký Realtime Logs ở đáy popup
    g_AudioLogger.Draw();

    // Khối thanh điều hướng tác vụ & Lưu trữ đĩa cứng ở chân trang (Footer)
    ImGui::Spacing();
    if (CSImGui::SecondaryButton("Xóa Nhật Ký Log", ImVec2(130, 28))) {
        g_AudioLogger.Clear();
    }
    ImGui::SameLine();
    if (CSImGui::ModernButton("Ghi Cấu Hình (Save JSON)", ImVec2(180, 28), true)) {
        afMgr.SaveToFile();
        g_AudioLogger.Log("[PASS] Đã đồng bộ toàn bộ pipeline xuống file JSON thành công.");
    }
    ImGui::SameLine();
    if (CSImGui::SecondaryButton("Nạp Từ File JSON", ImVec2(140, 28))) {
        afMgr.LoadFromFile();
        g_AudioLogger.Log("[PASS] Đã ép nạp lại dữ liệu từ tệp cấu hình JSON.");
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 95.0f);
    if (CSImGui::SecondaryButton("Đóng", ImVec2(80, 28))) {
        closePopup_AudioControl = true;
    }
}

void OpenAudioControlPopup(ReusablePopup& popup) {
    CSImGui::PushModernWindowStyle();
    popup.Open("Hệ Thống Quản Lý & Tinh Chỉnh Âm Thanh", [](bool& closePopup_AudioControl) {
        ShowAudioControlPopup(closePopup_AudioControl);  
    });
    CSImGui::PopModernWindowStyle();
}

void RenderAudioControlPopup(ReusablePopup& popup) {
    if (popup.IsOpen()) {
        popup.Render();
    }
}