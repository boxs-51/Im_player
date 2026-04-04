#include "mpv/mpv_settings.h"
#include "mpv/mpv_custom_ui.h"
#include "mpv/shaders/shaders_manager.h"

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

void GeneralSettingsPage() {
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Bắt đầu vùng chứa chính
    CusTomImGui::BeginModernChild("GeneralSettingsPanel", avail, true);
    {
        static float scale = 1.0f;
        // --- TIÊU ĐỀ PHÂN ĐOẠN ---
        ImGui::TextColored(GTheme.TextLaBel_NormalCombo, " CẤU HÌNH GIAO DIỆN HỆ THỐNG");
        
        ImGui::TextDisabled("Thay đổi giao diện và kích thước hiển thị để phù hợp với trải nghiệm của bạn.");
        ImGui::Separator();
        ImGui::Spacing(); ImGui::Spacing();

        // Sử dụng Group để quản lý layout dòng đầu tiên
        ImGui::BeginGroup();
        {
            // 1. COMBO CHỌN THEME
            static std::string selectedTheme = ThemeToString(c_Settings.themetype);
            std::vector<std::string> themes = { "Dark Mode", "Light Mode", "Mid Night Mode", "Retro Mode" };

            ImGui::BeginGroup(); // Nhóm Label + Combo
            if (CusTomImGui::NormalCombo("CHỦ ĐỀ (THEME)", selectedTheme, themes, 220, 4)) {
                if (selectedTheme == "Dark Mode") c_Settings.themetype = ThemeType::DarkMode;
                else if (selectedTheme == "Light Mode") c_Settings.themetype = ThemeType::LightMode;
                else if (selectedTheme == "Mid Night Mode") c_Settings.themetype = ThemeType::MidnightMode;
                else if (selectedTheme == "Retro Mode") c_Settings.themetype = ThemeType::RetroMode;

                ApplyTheme(c_Settings.themetype);
                SaveSettings_Common();
            }
            ImGui::TextDisabled("Thay đổi màu sắc tổng thể (Sáng/Tối).");
            ImGui::EndGroup();

            ImGui::SameLine(0, 30); // Khoảng cách giữa 2 combo là 30px

            // 2. COMBO SEARCH CHỌN FONT SIZE (CÓ PREVIEW)
            static std::string fontSizeStr = std::to_string((int)c_Settings.fontsize) + "px";
            static std::vector<std::string> fontSizes = {
                "12px", "14px", "16px", "18px", "20px", "24px", "28px", "32px"
            };

            auto validateFont = [](std::string& input) -> bool {
                try {
                    // Tìm số trong chuỗi (xử lý cả khi người dùng gõ "20" hoặc "20px")
                    int val = std::stoi(input); 
                    if (val >= 8 && val <= 40) {
                        input = std::to_string(val) + "px"; // Chuẩn hóa lại chuỗi
                        return true; 
                    }
                } catch (...) {}
                return false; // Trả về false để hủy bỏ thay đổi (Reset)
            };
            // Logic 2: Tạo Option ảo khi đang gõ (Dùng cho UI danh sách)
            auto getDynamicFont = [](const std::string& typing) -> std::string {
                try {
                    if (typing.empty()) return "";
                    int val = std::stoi(typing);
                    if (val >= 8 && val <= 40) {
                        return std::to_string(val) + "px"; // Hiện "15px" trong list khi gõ "15"
                    }
                } catch (...) {}
                return "";
            };

            ImGui::BeginGroup();
            // ModernSearchCombo đã được tích hợp logic font->Scale bên trong như đã thảo luận
            if (CusTomImGui::ModernSearchCombo("KÍCH THƯỚC CHỮ (FONT SIZE)", fontSizeStr, fontSizes, 180, 6 ,validateFont,getDynamicFont)) {
                try {
                    size_t pxPos = fontSizeStr.find("px");
                    std::string numericPart = (pxPos != std::string::npos) ? fontSizeStr.substr(0, pxPos) : fontSizeStr;
                    
                    float newSize = std::stof(numericPart);
                    if (newSize >= 8.0f && newSize <= 40.0f) { 
                        c_Settings.fontsize = newSize;
                        SaveSettings_Common();
                    }

                } catch (...) {

                }
            }
            ImGui::TextDisabled("Phóng to/thu nhỏ văn bản hệ thống.");

            ImGui::EndGroup();
        }
        ImGui::EndGroup();

    }
    CusTomImGui::EndModernChild();
}

void ShaderSettingsPage() {

    ImVec2 avail = ImGui::GetContentRegionAvail();
    auto& sm = ShaderManager::Instance();
    
    if (CusTomImGui::BeginModernChild("ShaderSettingsPanel", avail, true)) {
        
        // --- HEADER & GIỚI THIỆU CHUNG ---
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "VIDEO POST-PROCESSING (GLSL)");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Shader là các bộ lọc xử lý hình ảnh trực tiếp bằng GPU.\n"
                              "Giúp tăng chất lượng video, khử nhiễu hoặc làm nét hình ảnh.");
        }
        ImGui::Separator();

        if (CusTomImGui::BeginModernTabBar("ShaderChildTabs")) {
                
            // --- TAB 1: THƯ VIỆN (LIBRARY) ---
            if (ImGui::BeginTabItem("Library")) {
                ImGui::TextWrapped("Chọn các Shader bên dưới để nạp vào Pipeline. Thứ tự nạp sẽ quyết định kết quả cuối cùng.");
                
                ImGui::Spacing();
                if (CusTomImGui::ModernButton("Enable All")) sm.EnableAll();
                ImGui::SameLine();
                if (CusTomImGui::SecondaryButton("Disable All")) sm.DisableAll();

                std::vector<CusTomImGui::TableCol> cols = {
                    {"", 40.0f}, 
                    {"Tên Shader", 180.0f},
                    {"Giai đoạn", 80.0f},
                    {"Công dụng & Chi tiết", 0.0f}
                };
                
                if (CusTomImGui::BeginListTable("ShaderListTable", cols, ImGuiTableFlags_ScrollY)) {
                    for (auto& [name, shader] : sm.GetShaders()) {
                        CusTomImGui::BeginListRow();
                        
                        bool enabled = shader.enabled;
                        if (CusTomImGui::ModernCheckbox(("##cb_" + name).c_str(), &enabled)) {
                            sm.Toggle(name);
                        }
                        
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(name.c_str());

                        ImGui::TableNextColumn();
                        const char* stages[] = {"NATIVE", "PREKERNEL", "POSTKERNEL", "LINEAR", "MAIN", "OUTPUT" ,"UNKNOWN"};
                        ImGui::TextDisabled("[%s]", stages[(int)shader.hook]);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Giai đoạn Hook: Xác định thời điểm shader can thiệp vào luồng render của MPV.");
                        }

                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", shader.description.empty() ? "Không có mô tả." : shader.description.c_str());

                        CusTomImGui::EndListRow();
                    }
                        
                    CusTomImGui::EndListTable();
                    
                }
                
                ImGui::EndTabItem();
            }

            // --- TAB 2: CẤU HÌNH (CONFIGURATION) ---
            if (ImGui::BeginTabItem("Configuration")) {
                ImGui::Columns(2, "ConfigSplit", true);
                ImGui::SetColumnWidth(0, 160.0f);

                ImGui::TextDisabled("ĐANG CHẠY");
                static std::string selectedName = "";

                if(CusTomImGui::BeginModernChild("PipeList", ImVec2(0, 0), false)){
                    for (const auto& name : sm.GetPipeline()) {
                        bool is_selected = (selectedName == name);
                        if (CusTomImGui::ModernSelectable(name.c_str(), is_selected)) {
                            selectedName = name;
                        }
                    }
                    CusTomImGui::EndModernChild();
                }

                ImGui::NextColumn();

                if (!selectedName.empty()) {
                    auto& s = sm.GetShaders()[selectedName];
                    CusTomImGui::BeginCard();
                    
                    ImGui::Text("Tùy chỉnh: %s", s.name.c_str());
                    ImGui::TextDisabled("Điều chỉnh các tham số bên dưới để thay đổi hiệu ứng hiển thị.");

                    if (CusTomImGui::BeginInfoTable("Meta", 2, 80.0f)) {
                        CusTomImGui::InfoRow("Đường dẫn", "%s", s.path.c_str());
                        CusTomImGui::InfoRow("Thứ tự", "Ưu tiên: %d", s.order);
                        CusTomImGui::EndInfoTable();
                    }

                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    bool changed = false;
                    for (auto& p : s.params) {
                        ImGui::Text("%s", p.label.empty() ? p.name.c_str() : p.label.c_str());
                        ImGui::SameLine();
                        ImGui::TextDisabled("(?)");
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Thông tin chi tiết: %s", p.info.c_str());

                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::SliderFloat(("##" + p.name).c_str(), &p.value, p.min, p.max, "%.2f")) {
                            changed = true;
                        }
                    }

                    //if (changed) sm.UpdateParams(s);
                    
                    if (CusTomImGui::SecondaryButton("Reset mặc định", ImVec2(-1, 0))) {
                        // Logic reset ở đây nếu cần
                    }
                    
                    CusTomImGui::EndCard();
                } else {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
                    ImGui::Indent(20);
                    ImGui::TextDisabled("Vui lòng chọn một Shader từ danh sách bên trái\nđể bắt đầu hiệu chỉnh thông số.");
                }
                ImGui::Columns(1);
                ImGui::EndTabItem();
            }
            CusTomImGui::EndModernTabBar();
        }
        CusTomImGui::EndModernChild();
    }
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
            
        if (CusTomImGui::BeginInfoTable("settings_layout")) {
            
            ImGui::TableNextRow();

            // ==== CỘT TRÁI (SIDEBAR) ====
            ImGui::TableSetColumnIndex(0);
            
            ImGui::Spacing();
            ImGui::TextDisabled("Systems"); // Phân loại mục cài đặt
            ImGui::Spacing();

            const char* items[] = { "General", "Shader ", "Options" };
            for (int i = 0; i < IM_ARRAYSIZE(items); ++i) {
                // Dùng ModernSelectable đã viết trước đó
                if (CusTomImGui::ModernSelectable(items[i], selectedItem == i)) {
                    selectedItem = i;
                }
            }
            
            // ==== CỘT PHẢI (CONTENT AREA) ====
            ImGui::TableSetColumnIndex(1);
            
            // Tạo một vùng Child bên phải để nội dung có thể cuộn độc lập
            if (CusTomImGui::BeginModernChild("##SettingContent", ImVec2(-1, -1), false)) {
                
                if (selectedItem == 0) {
                    ImGui::TextDisabled("General Settings");
                    ImGui::Separator();
                    GeneralSettingsPage();
                    ImGui::Spacing();
                } 
                else if(selectedItem == 1) {
                    ImGui::TextDisabled("Quan ly Shader");
                    ImGui::Separator();
                    ShaderSettingsPage();
                    ImGui::Spacing();
                }
                else if (selectedItem == 2) {
                    ImGui::TextDisabled("TÙY CHỌN CHUNG");
                    ImGui::Separator();

                    ImGui::Spacing();
                    
                }
                CusTomImGui::EndModernChild();
            }
            

            CusTomImGui::EndInfoTable();
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
