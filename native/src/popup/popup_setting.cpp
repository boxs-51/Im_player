#include "mpv/mpv_settings.h"
#include "mpv/mpv_custom_ui.h"
#include "globals.h"
#include "utils.h"
#include "popup_setting.h"
#include "FontManager.h"

#include <imgui.h>
#include <vector>
#include <string>

static int tempFontIndex = 0; // font chọn tạm thời
static std::string tempFamily;
static std::string tempStyle;
static int selectedItem = 0; 
static bool temprunPopup_setting = false;

// Mở popup settings
void OpenSettingPopup() {
    temprunPopup_setting = true;
    SettingPopup.Open("Settings", [](bool& closePopup_setting) {
        ShowSettingsPopup(closePopup_setting);
    });
}

void GeneralSettingsPage(){

    ImVec2 avail = ImGui::GetContentRegionAvail();

    CusTomImGui::BeginModernChild("LeftPanel", avail, true);
    {
        ImGui::Text("Tùy chọn hiển thị");
        ImGui::Separator();
        
        // Dùng ModernCombo vừa tạo
        static std::string selectedTheme = ThemeToString(c_Settings.themetype);
        std::vector<std::string> themes = { "Dark Mode", "Light Mode", "Nord Mode", "Cyberpunk Mode", "Dracula Mode" };
        
        if (CusTomImGui::NormalCombo("CHỦ ĐỀ (THEME)", selectedTheme, themes , 3)) {
            // Logic đổi theme mượt mà của bạn ở đây
            if (selectedTheme == "Dark Mode") {
                c_Settings.themetype = ThemeType::DarkMode;
                ApplyTheme(c_Settings.themetype);
                SaveSettings_Common();
            }
            else if (selectedTheme == "Light Mode") {
                c_Settings.themetype = ThemeType::LightMode;
                ApplyTheme(c_Settings.themetype);
                SaveSettings_Common();
            }

        }
    }
    CusTomImGui::EndModernChild();

}

// Hàm render nội dung chính của popup
void ShowSettingsPopup(bool& closePopup_setting) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    
    // Sử dụng chiều cao cố định để dành chỗ cho hàng nút bấm ở dưới
    float footerHeight = 50.0f;
    ImVec2 contentSize = ImVec2(avail.x, avail.y - footerHeight);

    // 1. Dùng BeginModernChild để bao bọc toàn bộ vùng nội dung
    if (CusTomImGui::BeginModernChild("##SettingsMain", contentSize, true)) {
        
        // 2. Thiết lập bảng chia Sidebar và Content (Không dùng viền Borders cứng nhắc)
        if (ImGui::BeginTable("settings_layout", 2, ImGuiTableFlags_NoSavedSettings)) {
            
            // Cột trái (Sidebar): Cố định 160px
            ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 160.0f);
            // Cột phải (Content): Co giãn hết cỡ
            ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();

            // ==== CỘT TRÁI (SIDEBAR) ====
            ImGui::TableSetColumnIndex(0);
            
            ImGui::Spacing();
            ImGui::TextDisabled("Systems"); // Phân loại mục cài đặt
            ImGui::Spacing();

            const char* items[] = { "General", "Options" };
            for (int i = 0; i < IM_ARRAYSIZE(items); ++i) {
                // Dùng ModernSelectable đã viết trước đó
                if (CusTomImGui::ModernSelectable(items[i], selectedItem == i)) {
                    selectedItem = i;
                }
            }
            
            // ==== CỘT PHẢI (CONTENT AREA) ====
            ImGui::TableSetColumnIndex(1);
            
            // Tạo một vùng Child bên phải để nội dung có thể cuộn độc lập
            if (ImGui::BeginChild("##SettingContent", ImVec2(-1, -1), false)) {
                
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
                
                if (selectedItem == 0) {
                    ImGui::TextDisabled("General Settings");
                    ImGui::Separator();
                    GeneralSettingsPage();
                    ImGui::Spacing();
                } 
                else if (selectedItem == 1) {
                    ImGui::TextDisabled("TÙY CHỌN CHUNG");
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Ví dụ dùng InfoTable bên trong Settings
                    if (CusTomImGui::BeginInfoTable("##OptionsTable", 250.0f)) {
                        CusTomImGui::InfoRow("Tự động lưu", "Bật");
                        CusTomImGui::InfoRow("Giao diện", "Dark Mode");
                        CusTomImGui::EndInfoTable();
                    }
                }

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
        CusTomImGui::EndModernChild();
    }

    // ==== FOOTER (BUTTONS) ====
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Căn phải các nút bấm
    float btnWidth = 100.0f;
    float totalBtnWidth = (btnWidth * 3) + (ImGui::GetStyle().ItemSpacing.x * 2);
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - totalBtnWidth - 20.0f);

    if (CusTomImGui::ModernButton("Lưu", ImVec2(btnWidth, 35))) {

    }

    ImGui::SameLine();
    if (CusTomImGui::SecondaryButton("Reset", ImVec2(btnWidth, 35))) {
  
    }

    ImGui::SameLine();
    if (CusTomImGui::SecondaryButton("Đóng", ImVec2(btnWidth, 35))) {
        closePopup_setting = true;
    }
}

// Render popup mỗi frame
void RenderSettingPopup() {
    SettingPopup.Render();
}
