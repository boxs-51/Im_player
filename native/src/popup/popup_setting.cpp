#include "mpv/mpv_settings.h"
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

// Hàm render nội dung chính của popup
void ShowSettingsPopup(bool& closePopup_setting) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 1.0f);

    // Nền xám nhạt cho child
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));

    // Style chung cho button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

    // Style text và border
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    // Style cho frame input
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

    // Style popup & header
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));

    ImGui::BeginChild("##Setting_Common", avail, true);

    // ==== Table 2 cột ====
    if (ImGui::BeginTable("settings_table", 2,
        ImGuiTableFlags_Resizable | 
        ImGuiTableFlags_BordersInnerV | 
        ImGuiTableFlags_BordersOuterV | 
        ImGuiTableFlags_RowBg)) 
    {
        ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);     
        ImGui::TableNextRow();

        // ==== Cột trái (label) ====
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Danh sách cài đặt");
        ImGui::Separator();

        // Danh sách các mục settings
        const char* items[] = { "Font", "Options" };
        for (int i = 0; i < IM_ARRAYSIZE(items); ++i) {
            bool isSelected = (selectedItem == i);

            // Highlight background cho mục đang chọn
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            }

            if (ImGui::Selectable(items[i], isSelected)) {
                selectedItem = i;
            }

            if (isSelected) {
                ImGui::PopStyleColor(2);
            }
        }

        // ==== Cột phải (content) ====
        ImGui::TableSetColumnIndex(1);
        if (selectedItem == 0) {
            RenderFontSettingsContent();
        } else if (selectedItem == 1) {
            ImGui::Text("Option settings here...");
        }

        ImGui::EndTable();
    }

    // ==== Buttons dưới cùng ====
    ImGui::Separator();

    if (ImGui::Button("Save Settings")) {
        FontManager::Instance().SetCurrentFont(tempFontIndex);
        c_Settings.defaultFamily = tempFamily;
        c_Settings.defaultStyle  = tempStyle;
        SaveSettings_Common();
    }

    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::Button("Reset Settings")) {
        // Reset logic
    }

    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::Button("Close")) {
        closePopup_setting = true;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(14);
    ImGui::PopStyleVar();
}
// Nội dung phần font settings
void RenderFontSettingsContent() {
    auto& fm = FontManager::Instance();
    const auto& fonts = fm.GetFonts();
    if (fonts.empty()) { ImGui::Text("No fonts loaded"); return; }

    // Gom nhãn hiển thị
    std::vector<std::string> displayNames;
    displayNames.reserve(fonts.size());
    for (const auto& f : fonts)
        displayNames.push_back(f.family + " " + f.style);

    // Khởi tạo lựa chọn tạm theo settings hiện tại
    if (tempFontIndex < 0 || tempFontIndex >= (int)fonts.size() || selectedItem > 0 || temprunPopup_setting ) {
        int found = -1;
        for (int i = 0; i < (int)fonts.size(); ++i)
            if (fonts[i].family == c_Settings.defaultFamily &&
                fonts[i].style  == c_Settings.defaultStyle) { found = i; break; }

        tempFontIndex = (found >= 0) ? found : 0;
        tempFamily = fonts[tempFontIndex].family;
        tempStyle  = fonts[tempFontIndex].style;
        temprunPopup_setting = false;
    } 



    ImGui::PushID("FontSettingsSection");

    // Combo label
    ImGui::TextUnformatted("Fonts :");
    ImGui::SameLine(0.0f, 5.0f);
    ImGui::SetNextItemWidth(-1);

    int currentFontIndex = fm.GetCurrentFontIndex();

    // Preview hiển thị font tạm
    std::string previewName = (tempFontIndex >= 0 && tempFontIndex < (int)fonts.size())
                              ? displayNames[tempFontIndex]
                              : "Unknown";

    if (ImGui::BeginCombo("##FontCombo", previewName.c_str(), ImGuiComboFlags_HeightLarge)) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        for (int i = 0; i < (int)displayNames.size(); ++i) {
            bool selected = (tempFontIndex == i);
            bool isCurrentFont = (i == currentFontIndex);

            ImGui::PushID(i);

            // Disable font đang áp dụng
            if (isCurrentFont)
                ImGui::Selectable(displayNames[i].c_str(), false, ImGuiSelectableFlags_Disabled);
            else if (ImGui::Selectable(displayNames[i].c_str(), selected)) {
                tempFontIndex = i;
                tempFamily = fonts[i].family;
                tempStyle  = fonts[i].style;
            }

            // Vẽ nền nhạt/đậm
            ImVec2 item_min = ImGui::GetItemRectMin();
            ImVec2 item_max = ImGui::GetItemRectMax();
            ImU32 baseColor = 0;

            if (isCurrentFont)
                baseColor = IM_COL32(200, 200, 200, 100); // font đang áp dụng, nhạt
            if (ImGui::IsItemHovered())
                baseColor = IM_COL32(180, 180, 180, 200); // hover, đậm hơn

            if (baseColor != 0)
                draw_list->AddRectFilled(item_min, item_max, baseColor, 0.0f);

            // Vẽ viền
            draw_list->AddRect(item_min, item_max, IM_COL32(150, 150, 150, 255), 0.0f, 0, 1.0f);

            // Auto scroll tới font đang áp dụng
            if (isCurrentFont)
                ImGui::SetItemDefaultFocus();

            if (selected)
                ImGui::SetItemDefaultFocus();

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    ImGui::Separator();

    // Preview font tạm
    if (tempFontIndex >= 0 && tempFontIndex < (int)fonts.size() && fonts[tempFontIndex].imFont) {
        ImGui::PushFont(fonts[tempFontIndex].imFont);
        ImGui::Text("This text uses the selected font!");
        ImGui::PopFont();
    }

    // Apply
    if (ImGui::Button("Apply ##FontApply")) {
        fm.SetCurrentFont(tempFontIndex);
        c_Settings.defaultFamily = tempFamily;
        c_Settings.defaultStyle  = tempStyle;
        SaveSettings_Common();
    }

    ImGui::PopID();
}

// Render popup mỗi frame
void RenderSettingPopup() {
    SettingPopup.Render();
}
