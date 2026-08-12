
#include <gui/gui.h>
#include "globals.h"
#include "utils.h"
#include "popup_setting.h"
#include "FontManager.h"
#include "settings_manager.h"

#include "windows/WindowRuntime.h"
#include "player/shaders/shaders_manager.h"


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
    popup.Open("Settings", [](WindowRuntime* runtime, bool& closePopup_setting) {
        ShowSettingsPopup(runtime, closePopup_setting);
    });
}

void GeneralSettingsPage() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    auto& Cfg = ConfigManager::Instance();
    auto commonCfg = Cfg.GetCommonSettings();
    // Bắt đầu vùng chứa chính
    if(CSImGui::BeginModernChild("GeneralSettingsPanel", avail, true))
    {
        static float scale = 1.0f;
        // --- TIÊU ĐỀ PHÂN ĐOẠN ---
        ImGui::Text(" CẤU HÌNH GIAO DIỆN HỆ THỐNG");
        
        ImGui::TextDisabled("Thay đổi giao diện và kích thước hiển thị để phù hợp với trải nghiệm của bạn.");
        ImGui::Separator();
        ImGui::Spacing(); ImGui::Spacing();

        // Sử dụng Group để quản lý layout dòng đầu tiên
        ImGui::BeginGroup();
        {
            // 1. COMBO CHỌN THEME
            std::string selectedTheme_str = ThemeToString(commonCfg.themetype);
            std::vector<std::string> themes = { "Dark Mode", "Light Mode", "Mid Night Mode", "Retro Mode" };

            ImGui::BeginGroup(); // Nhóm Label + Combo
            if (CSImGui::NormalCombo("CHỦ ĐỀ (THEME)", selectedTheme_str, themes, 220, 4)) {
                ThemeType selectedTheme = StringToTheme(selectedTheme_str);
                Cfg.UpdateCommonSettings([selectedTheme](CommonSettings& settings) {
                    settings.themetype = selectedTheme;
                });
                CSImGui::ApplyTheme(selectedTheme);
                Cfg.SaveCommon();
            }
            ImGui::TextDisabled("Thay đổi màu sắc tổng thể (Sáng/Tối).");
            ImGui::EndGroup();

            ImGui::SameLine(0, 30); // Khoảng cách giữa 2 combo là 30px

            // 2. COMBO SEARCH CHỌN FONT SIZE (CÓ PREVIEW)
            static std::string fontSizeStr = std::to_string((int)commonCfg.fontsize) + "px";
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
            if (CSImGui::ModernSearchCombo("KÍCH THƯỚC CHỮ (FONT SIZE)", fontSizeStr, fontSizes, 180, 6 ,validateFont,getDynamicFont)) {
                try {
                    size_t pxPos = fontSizeStr.find("px");
                    std::string numericPart = (pxPos != std::string::npos) ? fontSizeStr.substr(0, pxPos) : fontSizeStr;
                    
                    float newSize = std::stof(numericPart);
                    if (newSize >= 8.0f && newSize <= 40.0f) { 
                        Cfg.UpdateCommonSettings([newSize](CommonSettings& settings) {
                            settings.fontsize = newSize;
                        });
                        Cfg.SaveCommon();
                    }

                } catch (...) {

                }
            }
            ImGui::TextDisabled("Phóng to/thu nhỏ văn bản hệ thống.");

            ImGui::EndGroup();
        }
        ImGui::EndGroup();
        CSImGui::EndModernChild();
    }
}

void ShaderSettingsPage(WindowRuntime* runtime) {

    auto* session = runtime->resource.GetPlayerSession();
    if (!session) return;
    auto& sm = *session->GetShaderManager();

    static bool wasConfigTabOpen = false;
    float scale = ImGui::GetStyle().FontScaleMain;
    ImVec2 avail = ImGui::GetContentRegionAvail();

    if (CSImGui::BeginModernChild("ShaderSettingsPanel", avail, true)) {
        
        // --- HEADER & GIỚI THIỆU CHUNG ---
        ImGui::Text("VIDEO POST-PROCESSING (GLSL)");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Shader là các bộ lọc xử lý hình ảnh trực tiếp bằng GPU.\n"
                              "Giúp tăng chất lượng video, khử nhiễu hoặc làm nét hình ảnh.");
        }

        ImGui::Separator();

        if (CSImGui::BeginModernTabBar("ShaderChildTabs")) {
            // --- TAB 1: THƯ VIỆN (LIBRARY) ---
            if (CSImGui::ModernTabItem("Library")) {
                ImGui::TextWrapped("Chọn các Shader bên dưới để nạp vào Pipeline. Thứ tự nạp sẽ quyết định kết quả cuối cùng.");
                
                ImGui::Spacing();
                if (CSImGui::ModernButton("Enable All")) sm.EnableAll();
                ImGui::SameLine();
                if (CSImGui::SecondaryButton("Disable All")) sm.DisableAll();
                ImGui::SameLine();
                if (CSImGui::ModernButton("Reload All")) sm.Reload(); 

                std::vector<CSImGui::TableCol> cols = {
                    {"", 40.0f * scale}, 
                    {"Tên Shader", 180.0f * scale},
                    {"Giai đoạn", 80.0f * scale},
                    {"Công dụng & Chi tiết", 0.0f * scale}
                };
                
                if (CSImGui::BeginListTable("ShaderListTable", cols, ImGuiTableFlags_ScrollY)) {
                    for (auto& [name, shader] : sm.GetShaders()) {
                        CSImGui::BeginListRow();
                        
                        bool enabled = shader.enabled;
                        if (CSImGui::ModernCheckbox(("##cb_" + name).c_str(), &enabled)) {
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

                        CSImGui::EndListRow();
                    }
                        
                    CSImGui::EndListTable();
                    
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
                float buttonWidth = 120.0f * scale;
                float spacing = 10.0f * scale;

                ImGui::SetNextItemWidth(availWidth - buttonWidth - spacing);
                CSImGui::ModernInputText("##path", folderPath, IM_ARRAYSIZE(folderPath));
                ImGui::SameLine();

                if (CSImGui::ModernButton("Add Folder", ImVec2(buttonWidth, 32))) {
                    if (strlen(folderPath) > 0) {
                        sm.AddFolder(folderPath);
                        memset(folderPath, 0, sizeof(folderPath));
                    }
                }

                ImGui::Spacing();

                // Danh sách folder hiện đại (Dùng ChildWindow cố định chiều cao để tránh đẩy UI đi quá xa)
                /*
                if (CSImGui::BeginModernChild("FolderList", ImVec2(0, 0), true)) {
                    std::string toRemove = ""; // Biến tạm để tránh lỗi iterator
                    const float rowHeight = 26.0f;
                    
                    for (const auto& path : sm.GetSearchPaths()) {

                        ImGui::PushID(path.c_str());

                        // Lấy tọa độ dòng hiện tại
                        ImVec2 p_min = ImGui::GetCursorScreenPos();
                        float fullWidth = ImGui::GetContentRegionAvail().x;
                        
                        // Vẽ Selectable tàng hình để tạo hiệu ứng hover cho cả dòng
                        CSImGui::ModernSelectable("##row", false, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(fullWidth, rowHeight));

                        // Căn giữa text theo chiều dọc
                        float textY = p_min.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;

                        // 1. Vẽ Icon hoặc ký hiệu đầu dòng
                        ImGui::GetWindowDrawList()->AddText(ImVec2(p_min.x + 5, textY), ImGui::GetColorU32(ImGuiCol_TextDisabled), "-");

                        // 2. Vẽ Path (Giới hạn vùng vẽ để không đè lên nút x)
                        ImGui::SetCursorScreenPos(ImVec2(p_min.x + 20, textY));
                        std::string texttrum = TextUtils::TruncateTextByPixels(path.c_str(),fullWidth - 60);
                        ImGui::TextUnformatted(texttrum.c_str());
                        CSImGui::ShowTooltipDelayed(path.c_str(),ImGui::IsItemHovered(),3.0f,path.c_str());

                        // 3. Nút xóa (Căn phải tuyệt đối)
                        ImVec2 btnSize = ImVec2(20, 20);
                        float btnY = p_min.y + (rowHeight - btnSize.y) * 0.5f;
                        ImGui::SetCursorScreenPos(ImVec2(p_min.x + fullWidth - 25, btnY));
                        
                        if (CSImGui::ModernButton("x",btnSize)) {
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
                    CSImGui::EndModernChild();
                }*/
                CSImGui::EndModernTabItem();
            }           

            // --- TAB 2: CẤU HÌNH (CONFIGURATION) ---
            bool isConfigOpen = CSImGui::ModernTabItem("Configuration");
            if (isConfigOpen) {
                float Selectablewight = 160.0f * scale;
                ImGui::Columns(2, "ConfigSplit", true);
                ImGui::SetColumnWidth(0, Selectablewight + 40.0f);

                ImGui::TextDisabled("ĐANG CHẠY");
                wasConfigTabOpen = true;
                static std::string selectedName = "";

                const auto& pipeline = sm.GetPipeline();
                auto it = std::find(pipeline.begin(), pipeline.end(), selectedName);
                if (it == pipeline.end()) {
                    selectedName = ""; // Reset nếu shader không còn hoạt động
                }

                if(CSImGui::BeginModernChild("PipeList", ImVec2(0, 0), false)){
                    for (const auto& name : pipeline) {
                        std::string text_trum = TextUtils::TruncateTextByPixels(name.c_str(),Selectablewight );
                        bool is_selected = (selectedName == name);
                        if (CSImGui::ModernSelectable(text_trum.c_str(), is_selected)) {
                            selectedName = name;
                            sm.DiscardChanges();
                            isDirty = false;
                        }
                    }
                    CSImGui::EndModernChild();
                }

                ImGui::NextColumn();

                if (!selectedName.empty()) {
                    auto& s = sm.GetShaders()[selectedName];
                    if(CSImGui::BeginCard()){
                    
                        ImGui::Text("Tùy chỉnh: %s", s.name.c_str());
                        ImGui::TextDisabled("Điều chỉnh các tham số bên dưới để thay đổi hiệu ứng hiển thị.");

                        if (CSImGui::BeginInfoTable("Meta", 2, 80.0f * scale)) {
                            CSImGui::InfoRow("Đường dẫn", "%s", s.path.c_str());
                            CSImGui::InfoRow("Thứ tự", "Ưu tiên: %d", s.order);
                            CSImGui::EndInfoTable();
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
                            float slider_height = 5.0f * scale ;    // Độ dày thanh trượt
                            float grab_size = 10.0f * scale;      // Bán kính nút kéo
                            float full_width = ImGui::GetContentRegionAvail().x;

                            if (CSImGui::ModernSliderFloat(
                                    p.name.c_str(),            // Tên hiển thị phía trên slider
                                    &p.temp_value,                      // Giá trị float*
                                    p.min,                              // Giá trị tối thiểu
                                    p.max,                              // Giá trị tối đa
                                    slider_height,                      // Chiều cao thanh trượt (Tham số mới)
                                    grab_size,                          // Kích thước nút kéo (Tham số mới)
                                    "%.2fx",                            // Định dạng hiển thị
                                    full_width,                         // Chiều rộng full vùng chứa
                                    SliderFlags_None
                            )){
                                isDirty = true;
                            }
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        // --- CÁC NÚT ĐIỀU KHIỂN ---
                        // 1. Nút Apply: Lưu thiết lập mới
                        if (CSImGui::ModernButton("Apply")) {
                            sm.ApplyChanges(selectedName);
                            isDirty = false;
                        }
                        ImGui::SameLine();
                        // 2. Nút Reset: Về mặc định
                        if (CSImGui::SecondaryButton("Reset Default")) {
                            sm.ResetToDefault(selectedName);
                            isDirty = false;
                        }
                        
                        CSImGui::EndCard();
                    }
                } else {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
                    ImGui::Indent(20);
                    ImGui::TextDisabled("Vui lòng chọn một Shader từ danh sách bên trái\nđể bắt đầu hiệu chỉnh thông số.");
                }
                ImGui::Columns(1);
                CSImGui::EndModernTabItem();
            }
            if (!isConfigOpen && wasConfigTabOpen) {
                if (isDirty ) {
                    sm.DiscardChanges(); 
                }
                isDirty = false;
                wasConfigTabOpen = false;
            }
            // --- TAB 3: PIPELINE (MODERN REORDERABLE) ---
            if (CSImGui::ModernTabItem("Pipeline")) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));

                auto activePipeline = sm.GetActivePipeline();
                const float rowHeight = 32.0f * scale; // Tăng nhẹ độ cao để thoải mái hơn
                const ImVec2 btnSize = ImVec2(24, 24);

                std::string moveUp = "", moveDown = "", toRemove = "";

                if (CSImGui::BeginModernChild("PipelineList", ImVec2(0, 0), false)) {
                    HookStage lastStage = HookStage::UNKNOWN;
                    int index = 0;

                    for (auto* s : activePipeline) {
                        // Hiển thị Stage Header
                        if (s->hook != lastStage) {
                            lastStage = s->hook;
                            ImGui::Spacing();
                            ImGui::TextDisabled("STAGE: %s", sm.HookStageToString(s->hook).c_str());
                            ImGui::Separator();
                        }

                        ImGui::PushID(s->name.c_str());

                        // 1. Lấy tọa độ bắt đầu dòng
                        ImVec2 p_min = ImGui::GetCursorScreenPos();
                        float fullWidth = ImGui::GetContentRegionAvail().x;
                        float rightPadding = 10.0f * scale;
                        float btnSpacing = 4.0f * scale;
                        float buttonsWidth = (btnSize.x * 3) + (btnSpacing * 2) + rightPadding;
                        float selectableWidth = fullWidth - buttonsWidth - 6.0f; // trừ thêm 6px khoảng hở an toàn

                        // 2. Định dạng chuỗi hiển thị (Order | Icon | Name)
                        char label[256];
                        snprintf(label, sizeof(label), "%02d | %02d  ◆  %s", index + 1, s->order, s->name.c_str());

                        // 3. Vẽ Selectable làm nền và chứa text luôn
                        // Lưu ý: Dùng ## để ID không bị trùng nếu s->name giống nhau, 
                        // nhưng ở đây ta dùng s->name làm ID chính qua PushID rồi.
                        CSImGui::ModernSelectable(label, false, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(selectableWidth, rowHeight));
                        
                        // QUAN TRỌNG: Cho phép các nút bấm vẽ sau đây chiếm quyền ưu tiên click
                        ImGui::SetItemAllowOverlap();

                        // 4. Vẽ các nút bấm đè lên trên Selectable
                        float btnY = p_min.y + (rowHeight - btnSize.y) * 0.5f;
                        float cursorX = p_min.x + fullWidth - rightPadding; // Lùi vào 10px từ lề phải

                        // Nút X (Xóa)
                        cursorX -= btnSize.x;
                        ImGui::SetCursorScreenPos(ImVec2(cursorX, btnY));
                        if (CSImGui::ModernButton("X", btnSize)) {
                            toRemove = s->name;
                        }

                        // Nút Down
                        cursorX -= (btnSize.x + btnSpacing);
                        ImGui::SetCursorScreenPos(ImVec2(cursorX, btnY));
                        if (CSImGui::ModernArrowButton("##down", ImGuiDir_Down, btnSize)) {
                            moveDown = s->name;
                        }

                        // Nút Up
                        cursorX -= (btnSize.x + btnSpacing);
                        ImGui::SetCursorScreenPos(ImVec2(cursorX, btnY));
                        if (CSImGui::ModernArrowButton("##up", ImGuiDir_Up, btnSize)) {
                            moveUp = s->name;
                        }

                        // 5. Kết thúc dòng: Đưa cursor xuống dưới hàng vừa vẽ
                        ImGui::SetCursorScreenPos(ImVec2(p_min.x, p_min.y + rowHeight));

                        ImGui::PopID();
                        index++;
                    }
                    CSImGui::EndModernChild();
                }

                // Apply Actions (Xử lý logic sau khi vẽ xong để tránh lỗi iterator)
                if (!moveUp.empty())   sm.MoveUp(moveUp);
                if (!moveDown.empty()) sm.MoveDown(moveDown);
                if (!toRemove.empty()) sm.Disable(toRemove);

                ImGui::PopStyleVar();
                CSImGui::EndModernTabItem();
            }
            CSImGui::EndModernTabBar();
        }
        CSImGui::EndModernChild();
    }
}
// Hàm render nội dung chính của popup
void ShowSettingsPopup(WindowRuntime* runtime, bool& closePopup_setting) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    
    // Sử dụng chiều cao cố định để dành chỗ cho hàng nút bấm ở dưới
    float footerHeight = 50.0f;
    ImVec2 contentSize = ImVec2(avail.x, avail.y - footerHeight);

    // 1. Dùng BeginModernChild để bao bọc toàn bộ vùng nội dung
    if (CSImGui::BeginModernChild("##SettingsMain", contentSize, true)) {
        
        // 2. Thiết lập bảng chia Sidebar và Content (Không dùng viền Borders cứng nhắc)
            
        if (CSImGui::BeginInfoTable("settings_layout")) {
            
            ImGui::TableNextRow();

            // ==== CỘT TRÁI (SIDEBAR) ====
            ImGui::TableSetColumnIndex(0);
            
            ImGui::Spacing();
            ImGui::TextDisabled("Systems"); // Phân loại mục cài đặt
            ImGui::Spacing();

            const char* items[] = { "General", "Shader ", "Options" };
            for (int i = 0; i < IM_ARRAYSIZE(items); ++i) {
                // Dùng ModernSelectable đã viết trước đó
                if (CSImGui::ModernSelectable(items[i], selectedItem == i)) {
                    selectedItem = i;
                }
            }
            
            // ==== CỘT PHẢI (CONTENT AREA) ====
            ImGui::TableSetColumnIndex(1);
            
            // Tạo một vùng Child bên phải để nội dung có thể cuộn độc lập
            if (CSImGui::BeginModernChild("##SettingContent", ImVec2(-1, -1), false)) {
                
                if (selectedItem == 0) {
                    ImGui::TextDisabled("General Settings");
                    ImGui::Separator();
                    GeneralSettingsPage();
                    ImGui::Spacing();
                } 
                else if(selectedItem == 1) {
                    ImGui::TextDisabled("Quan ly Shader");
                    ImGui::Separator();
                    ShaderSettingsPage(runtime);
                    ImGui::Spacing();
                }
                else if (selectedItem == 2) {
                    ImGui::TextDisabled("TÙY CHỌN CHUNG");
                    ImGui::Separator();

                    ImGui::Spacing();
                    
                }
                CSImGui::EndModernChild();
            }
            

            CSImGui::EndInfoTable();
        }
        CSImGui::EndModernChild();
    }

    // ==== FOOTER (BUTTONS) ====
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Căn phải các nút bấm
    float btnWidth = 100.0f;
    float totalBtnWidth = (btnWidth * 3) + (ImGui::GetStyle().ItemSpacing.x * 2);
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - totalBtnWidth - 20.0f);

    if (CSImGui::ModernButton("Lưu", ImVec2(btnWidth, 35))) {

    }

    ImGui::SameLine();
    if (CSImGui::SecondaryButton("Reset", ImVec2(btnWidth, 35))) {
  
    }

    ImGui::SameLine();
    if (CSImGui::SecondaryButton("Đóng", ImVec2(btnWidth, 35))) {
        closePopup_setting = true;
    }
}

// Render popup mỗi frame
void RenderSettingPopup(ReusablePopup& popup, WindowRuntime* window) {
    if(popup.IsOpen()) {
        popup.Render(window);
    }
}
