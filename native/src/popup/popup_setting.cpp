#include "mpv/mpv_ui.h"
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
static bool isDirty = false;
// Mở popup settings
void OpenSettingPopup(ReusablePopup& popup) {
    popup.Open("Settings", [](bool& closePopup_setting) {
        ShowSettingsPopup(closePopup_setting);
    });
}

void GeneralSettingsPage() {
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Bắt đầu vùng chứa chính
    if(CusTomImGui::BeginModernChild("GeneralSettingsPanel", avail, true))
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
        CusTomImGui::EndModernChild();
    }
}

void ShaderSettingsPage() {
    static bool wasConfigTabOpen = false;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    auto& sm = ShaderManager::Instance();
    
    if (CusTomImGui::BeginModernChild("ShaderSettingsPanel", avail, true)) {
        
        // --- HEADER & GIỚI THIỆU CHUNG ---
        ImGui::Text("VIDEO POST-PROCESSING (GLSL)");
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
                ImGui::SameLine();
                if (CusTomImGui::ModernButton("Reload All")) sm.Reload(); 

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
                        ImGui::TextDisabled("[%s]", (sm.HookStageToString(shader.hook)).c_str());
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Giai đoạn Hook: Xác định thời điểm shader can thiệp vào luồng render của MPV.");
                        }

                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", shader.description.empty() ? "Không có mô tả." : shader.description.c_str());

                        CusTomImGui::EndListRow();
                    }
                        
                    CusTomImGui::EndListTable();
                    
                }
                
                // --- PHẦN QUẢN LÝ THƯ MỤC TRONG TAB LIBRARY ---
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "SHADERS SOURCE DIRECTORIES");
                ImGui::Spacing();

                // Khu vực Input chân trang
                static char folderPath[512] = "";
                float availWidth = ImGui::GetContentRegionAvail().x;
                float buttonWidth = 120.0f;
                float spacing = 10.0f;

                ImGui::SetNextItemWidth(availWidth - buttonWidth - spacing);
                CusTomImGui::ModernInputText("##path", folderPath, IM_ARRAYSIZE(folderPath));
                ImGui::SameLine();

                if (CusTomImGui::ModernButton("Add Folder", ImVec2(buttonWidth, 32))) {
                    if (strlen(folderPath) > 0) {
                        sm.AddFolder(folderPath);
                        memset(folderPath, 0, sizeof(folderPath));
                    }
                }

                ImGui::Spacing();

                // Danh sách folder hiện đại (Dùng ChildWindow cố định chiều cao để tránh đẩy UI đi quá xa)
                /*
                if (CusTomImGui::BeginModernChild("FolderList", ImVec2(0, 0), true)) {
                    std::string toRemove = ""; // Biến tạm để tránh lỗi iterator
                    const float rowHeight = 26.0f;
                    
                    for (const auto& path : sm.GetSearchPaths()) {

                        ImGui::PushID(path.c_str());

                        // Lấy tọa độ dòng hiện tại
                        ImVec2 p_min = ImGui::GetCursorScreenPos();
                        float fullWidth = ImGui::GetContentRegionAvail().x;
                        
                        // Vẽ Selectable tàng hình để tạo hiệu ứng hover cho cả dòng
                        CusTomImGui::ModernSelectable("##row", false, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(fullWidth, rowHeight));

                        // Căn giữa text theo chiều dọc
                        float textY = p_min.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;

                        // 1. Vẽ Icon hoặc ký hiệu đầu dòng
                        ImGui::GetWindowDrawList()->AddText(ImVec2(p_min.x + 5, textY), ImGui::GetColorU32(ImGuiCol_TextDisabled), "-");

                        // 2. Vẽ Path (Giới hạn vùng vẽ để không đè lên nút x)
                        ImGui::SetCursorScreenPos(ImVec2(p_min.x + 20, textY));
                        std::string texttrum = TextUtils::TruncateTextByPixels(path.c_str(),fullWidth - 60);
                        ImGui::TextUnformatted(texttrum.c_str());
                        ShowTooltipDelayed(path.c_str(),ImGui::IsItemHovered(),3.0f,path.c_str());

                        // 3. Nút xóa (Căn phải tuyệt đối)
                        ImVec2 btnSize = ImVec2(20, 20);
                        float btnY = p_min.y + (rowHeight - btnSize.y) * 0.5f;
                        ImGui::SetCursorScreenPos(ImVec2(p_min.x + fullWidth - 25, btnY));
                        
                        if (CusTomImGui::ModernButton("x",btnSize)) {
                            toRemove = path; // Chỉ đánh dấu, xóa sau
                        }

                        // Đăng ký diện tích dòng với ImGui
                        ImGui::SetCursorScreenPos(ImVec2(p_min.x, p_min.y + rowHeight));
                        ImGui::Dummy(ImVec2(fullWidth, 2)); // Tạo khoảng cách nhỏ giữa các dòng

                        ImGui::PopID();
                    }
                    
                    // Thực hiện xóa sau khi kết thúc vòng lặp
                    if (!toRemove.empty()) {
                        sm.RemoveFolder(toRemove);
                    }
                    CusTomImGui::EndModernChild();
                }*/
                ImGui::EndTabItem();
            }           

            // --- TAB 2: CẤU HÌNH (CONFIGURATION) ---
            bool isConfigOpen = ImGui::BeginTabItem("Configuration");
            if (isConfigOpen) {
                ImGui::Columns(2, "ConfigSplit", true);
                ImGui::SetColumnWidth(0, 160.0f);

                ImGui::TextDisabled("ĐANG CHẠY");
                wasConfigTabOpen = true;
                static std::string selectedName = "";

                const auto& pipeline = sm.GetPipeline();
                auto it = std::find(pipeline.begin(), pipeline.end(), selectedName);
                if (it == pipeline.end()) {
                    selectedName = ""; // Reset nếu shader không còn hoạt động
                }

                if(CusTomImGui::BeginModernChild("PipeList", ImVec2(0, 0), false)){
                    for (const auto& name : pipeline) {
                        bool is_selected = (selectedName == name);
                        if (CusTomImGui::ModernSelectable(name.c_str(), is_selected)) {
                            selectedName = name;
                            sm.DiscardChanges();
                            isDirty = false;
                        }
                    }
                    CusTomImGui::EndModernChild();
                }

                ImGui::NextColumn();

                if (!selectedName.empty()) {
                    auto& s = sm.GetShaders()[selectedName];
                    if(CusTomImGui::BeginCard()){
                    
                        ImGui::Text("Tùy chỉnh: %s", s.name.c_str());
                        ImGui::TextDisabled("Điều chỉnh các tham số bên dưới để thay đổi hiệu ứng hiển thị.");

                        if (CusTomImGui::BeginInfoTable("Meta", 2, 80.0f)) {
                            CusTomImGui::InfoRow("Đường dẫn", "%s", s.path.c_str());
                            CusTomImGui::InfoRow("Thứ tự", "Ưu tiên: %d", s.order);
                            CusTomImGui::EndInfoTable();
                        }

                        ImGui::Separator();
                        ImGui::Spacing();
                        
                        for (auto& p : s.params) {
                            ImGui::Text("%s", p.label.empty() ? p.name.c_str() : p.label.c_str());
                            ImGui::SameLine();
                            ImGui::TextDisabled("(?)");
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Thông tin chi tiết: %s", p.info.c_str());

                            ImGui::SetNextItemWidth(-1);
                            // Tính toán kích thước dựa trên scale để đồng nhất với giao diện chung
                            float slider_height = 5.0f ;    // Độ dày thanh trượt
                            float grab_size = 10.0f  ;      // Bán kính nút kéo
                            float full_width = ImGui::GetContentRegionAvail().x;

                            if (CusTomImGui::ModernSliderFloat(
                                    p.name.c_str(),            // Tên hiển thị phía trên slider
                                    &p.temp_value,                      // Giá trị float*
                                    p.min,                              // Giá trị tối thiểu
                                    p.max,                              // Giá trị tối đa
                                    slider_height,                      // Chiều cao thanh trượt (Tham số mới)
                                    grab_size,                          // Kích thước nút kéo (Tham số mới)
                                    "%.2fx",                            // Định dạng hiển thị
                                    full_width                          // Chiều rộng full vùng chứa
                            )){
                                isDirty = true;
                            }
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        // --- CÁC NÚT ĐIỀU KHIỂN ---
                        // 1. Nút Apply: Lưu thiết lập mới
                        if (CusTomImGui::ModernButton("Apply")) {
                            sm.ApplyChanges(selectedName);
                            isDirty = false;
                        }
                        ImGui::SameLine();
                        // 2. Nút Reset: Về mặc định
                        if (CusTomImGui::SecondaryButton("Reset Default")) {
                            sm.ResetToDefault(selectedName);
                            isDirty = false;
                        }
                        
                        CusTomImGui::EndCard();
                    }
                } else {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
                    ImGui::Indent(20);
                    ImGui::TextDisabled("Vui lòng chọn một Shader từ danh sách bên trái\nđể bắt đầu hiệu chỉnh thông số.");
                }
                ImGui::Columns(1);
                ImGui::EndTabItem();
            }
            if (!isConfigOpen && wasConfigTabOpen) {
                if (isDirty ) {
                    sm.DiscardChanges(); 
                }
                isDirty = false;
                wasConfigTabOpen = false;
            }
            // --- TAB 3: PIPELINE (MODERN REORDERABLE) ---
            if (ImGui::BeginTabItem("Pipeline")) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));
                
                auto activePipeline = sm.GetActivePipeline();
                const float rowHeight = 28.0f; // Độ cao dòng cố định để dễ căn chỉnh
                const ImVec2 btnSize = ImVec2(24, 24); // Nút nhỏ hơn rowHeight một chút để tạo khoảng thở

                std::string moveUp = "", moveDown = "", toRemove = "";

                if (CusTomImGui::BeginModernChild("PipelineList", ImVec2(0, 0), false)) {
                    HookStage lastStage = HookStage::UNKNOWN;
                    int index = 0;

                    for (auto* s : activePipeline) {

                        if (s->hook != lastStage) {
                            lastStage = s->hook;
                            ImGui::Spacing();
                            ImGui::TextDisabled("STAGE: %s", sm.HookStageToString(s->hook).c_str());
                            ImGui::Separator();
                        }

                        ImGui::PushID(s->name.c_str());

                        ImVec2 p_min = ImGui::GetCursorScreenPos();
                        float fullWidth = ImGui::GetContentRegionAvail().x;
                        ImVec2 p_max = ImVec2(p_min.x + fullWidth, p_min.y + rowHeight);

                        // ===== BG =====
                        CusTomImGui::ModernSelectable("##row", false, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(fullWidth, rowHeight));

                        // ===== CENTERING =====
                        float textY = p_min.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
                        float btnY  = p_min.y + (rowHeight - btnSize.y) * 0.5f;

                        float paddingX = 10.0f;
                        float spacing  = 6.0f;

                        // ===== RIGHT SIDE (BUTTONS) =====
                        float cursorX = p_max.x - paddingX;

                        // X
                        cursorX -= btnSize.x;
                        ImGui::SetCursorScreenPos(ImVec2(cursorX, btnY));
                        if (CusTomImGui::ModernButton("X", btnSize)) {
                            toRemove = s->name;
                        }

                        // Down
                        cursorX -= (btnSize.x + spacing);
                        ImGui::SetCursorScreenPos(ImVec2(cursorX, btnY));
                        if (CusTomImGui::ModernArrowButton("##down", ImGuiDir_Down, btnSize)) {
                            moveDown = s->name;
                        }

                        // Up
                        cursorX -= (btnSize.x + spacing);
                        ImGui::SetCursorScreenPos(ImVec2(cursorX, btnY));
                        if (CusTomImGui::ModernArrowButton("##up", ImGuiDir_Up, btnSize)) {
                            moveUp = s->name;
                        }

                        float buttonsStartX = cursorX;

                        // ===== LEFT SIDE =====

                        float x = p_min.x + paddingX;

                        // 🔢 ORDER
                        char orderBuf[16];
                        snprintf(orderBuf, sizeof(orderBuf), "%02d | %02d", index + 1, s->order);

                        ImGui::GetWindowDrawList()->AddText(
                            ImVec2(x, textY),
                            ImGui::GetColorU32(ImGuiCol_TextDisabled),
                            orderBuf
                        );

                        x += 48.0f; // width order

                        // 🔷 ICON
                        ImGui::GetWindowDrawList()->AddText(
                            ImVec2(x, textY),
                            ImGui::GetColorU32(ImGuiCol_TextDisabled),
                            "◆"
                        );

                        x += 18.0f;

                        // ===== TEXT (TRUNCATE) =====

                        float textMaxWidth = buttonsStartX - x - 6.0f;

                        std::string truncated = TextUtils::TruncateTextByPixels(
                            s->name.c_str(),
                            textMaxWidth
                        );

                        ImGui::SetCursorScreenPos(ImVec2(x, textY));
                        ImGui::TextUnformatted(truncated.c_str());

                        // ===== LAYOUT FIX =====
                        ImGui::SetCursorScreenPos(ImVec2(p_min.x, p_min.y + rowHeight));
                        ImGui::Dummy(ImVec2(fullWidth, rowHeight));

                        ImGui::PopID();
                        index++;
                    }
                    CusTomImGui::EndModernChild();
                }

                // Apply Actions
                if (!moveUp.empty())   sm.MoveUp(moveUp);
                if (!moveDown.empty()) sm.MoveDown(moveDown);
                if (!toRemove.empty()) sm.Disable(toRemove);

                ImGui::PopStyleVar();
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
void RenderSettingPopup(ReusablePopup& popup) {
    popup.Render();
}
