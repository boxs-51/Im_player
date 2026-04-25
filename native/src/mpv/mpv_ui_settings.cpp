#include "mpv/mpv_settings.h"   
#include "mpv/mpv_ui_settings.h"
#include "mpv/mpv_controller.h"
#include "mpv/mpv_basic_formats.h"
#include "mpv/mpv_ui.h"
#include <gui/gui.h>
#include <mpv/mpv_data.h>

#include "threads/thread_manager.h"

#include "globals.h"
#include "utils.h"
#include "json.hpp"


#include <imgui.h>
#undef RATE_LIMITED_COUT
#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>
#include <string>

static VideoAudioFormats& all_formats = GetVideoAudioFormats();
static MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
static VideoInfo& g_videoInfo = GetVideoInfo();
enum class SettingsPage { Main, ResolutionQuality, AudioQuality, PlaybackSpeed ,Options };
enum class OptionsPage { Main, Subtitles };
static SettingsPage current_page = SettingsPage::Main;
static OptionsPage current_options_page = OptionsPage::Main;

// Thêm biến global hoặc static bên trong RenderIOCHSidebar
static float page_anim = 1.0f; 
static SettingsPage last_page = SettingsPage::Main;

// Helper để đổi trang với hiệu ứng reset animation
auto ChangePage = [&](SettingsPage next) {
    if (current_page != next) {
        last_page = current_page;
        current_page = next;
        page_anim = 0.0f; // Reset animation về 0 để bắt đầu fade in
    }
};

// Thêm màu sắc và khoảng cách chuẩn
void UI_MenuItem(const char* label, const char* current_value, float scale, std::function<void()> on_click) {
    ImGui::PushID(label);
    
    float full_width = ImGui::GetContentRegionAvail().x;
    float item_height = 40.0f * scale;
    ImVec2 size = ImVec2(full_width, item_height); 

    // 1. Gọi ModernSelectable nâng cao
    // Lưu ý: Chúng ta không truyền nhãn vào đây để tránh nó vẽ chữ mặc định ở giữa
    // Hoặc truyền "" và tự vẽ để kiểm soát vị trí chính xác của label và value
    if (CSImGui::ModernSelectable("##item", false, 0, size)) {
        on_click();
    }

    // Lấy ID và Storage để lấy giá trị animation đồng bộ với ModernSelectable
    ImGuiID id = ImGui::GetItemID();
    float tHover = ImGui::GetStateStorage()->GetFloat(id, 0.0f);

    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // 2. Tính toán hiệu ứng trượt (Slide) đồng bộ
    float slide_offset = tHover * (4.0f * scale);
    float margin_left = (12.0f * scale) + slide_offset;
    float arrow_space = 35.0f * scale;
    float center_split = full_width * 0.55f;

    // 3. Vẽ Label (Có hiệu ứng trượt)
    float max_label_w = center_split - margin_left - (5.0f * scale);
    std::string safe_label = TextUtils::TruncateToWidth(label, max_label_w);
    draw_list->AddText(ImVec2(p_min.x + margin_left, p_min.y + (item_height - ImGui::GetFontSize()) * 0.5f), 
                       ToIUCol32(CSImGui::GetColors(Col_Text)), safe_label.c_str());

    // 4. Vẽ Value (Màu nhạt)
    if (current_value && strlen(current_value) > 0) {
        float max_val_w = (full_width - center_split) - arrow_space - (5.0f * scale);
        std::string safe_value = TextUtils::TruncateToWidth(current_value, max_val_w);
        float val_text_width = ImGui::CalcTextSize(safe_value.c_str()).x;
        
        draw_list->AddText(ImVec2(p_max.x - val_text_width - arrow_space, p_min.y + (item_height - ImGui::GetFontSize()) * 0.5f), 
                           ToIUCol32(CSImGui::GetColors(Col_TextDisabled)), safe_value.c_str());
    }
    
    // 5. Mũi tên (Dùng màu nhấn khi hover)
    ImU32 arrow_col = tHover > 0.5f ? ToIUCol32(CSImGui::GetColors(Col_Button)) : IM_COL32(100, 100, 100, 255);
    draw_list->AddText(ImVec2(p_max.x - (20.0f * scale), p_min.y + (item_height - ImGui::GetFontSize()) * 0.5f), 
                       arrow_col, ">");
    
    ImGui::PopID();
}
void UI_Toggle(const char* label, bool* v, float scale, bool enabled, std::function<void(bool)> on_change) {
    ImGui::PushID(label);
    
    float full_width = ImGui::GetContentRegionAvail().x;
    float row_height = 35.0f * scale;
    float spacing_y = 2.0f * scale; // Khoảng cách giữa các hàng
    
    ImVec2 size = ImVec2(full_width, row_height);

    // 1. Trước khi vẽ, hãy đảm bảo Cursor ở đúng vị trí
    ImVec2 p_min = ImGui::GetCursorScreenPos();

    // 2. Gọi ModernSelectable (Hàm này đã có ItemSize bên trong nên nó sẽ đăng ký vùng chiếm chỗ)
    if (CSImGui::ModernSelectable("##bg", false, 0, size , enabled) && enabled) {
        *v = !*v;
        if (on_change) on_change(*v);
    }

    // Lấy ID và thông số animation sau khi Selectable đã chạy
    ImGuiID id = ImGui::GetItemID();
    float tHover = ImGui::GetStateStorage()->GetFloat(id, 0.0f);
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // 3. Vẽ Label (Slide offset)
    float slide_offset = tHover * (4.0f * scale);
    float sw_w = 36.0f * scale;
    float sw_h = 18.0f * scale;
    
    float text_y_pos = p_min.y + (row_height - ImGui::GetFontSize()) * 0.5f;
    ImU32 text_col = enabled ? ToIUCol32(CSImGui::GetColors(Col_Text)) : ToIUCol32(CSImGui::GetColors(Col_TextDisabled));
    
    draw_list->AddText(ImVec2(p_min.x + (12.0f * scale) + slide_offset, text_y_pos), 
                       text_col, label);

    // 4. Vẽ ModernToggle (Dùng tọa độ tuyệt đối để không làm lệch Cursor của ImGui)
    ImVec2 sw_pos = ImVec2(p_max.x - sw_w - 12.0f * scale, p_min.y + (row_height - sw_h) * 0.5f);
    
    // Lưu vị trí cũ để khôi phục sau khi vẽ Toggle
    ImVec2 backup_cursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(sw_pos);
    
    // Gọi ModernToggle (Lưu ý dùng ID khác để tránh xung đột input với Selectable)
    CSImGui::ModernToggle("##sw_internal", v, enabled, scale);
    
    // 5. QUAN TRỌNG: Khôi phục Cursor và thêm khoảng cách dọc
    ImGui::SetCursorScreenPos(backup_cursor); 
    ImGui::Dummy(ImVec2(0.0f, spacing_y)); // Tạo khoảng trống giả để đẩy item tiếp theo xuống

    ImGui::PopID();
}
void UI_GroupHeader(const char* title, float scale = 1.0f) {
    ImGui::Spacing();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float height = ImGui::GetFontSize() + (4.0f * scale);

    // Vẽ thanh chỉ báo dọc (Indicator bar)
    draw_list->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + 3.0f * scale, p.y + height), ToIUCol32(CSImGui::GetColors(Col_CheckMark)), 2.0f);

    // Vẽ Title
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_Text, CSImGui::GetColors(Col_Text));
    ImGui::Text(title);
    ImGui::PopStyleColor();

    // Separator mờ dần hoặc màu mỏng
    ImVec4 sep_col = CSImGui::GetColors(Col_Separator);
    sep_col.w = 0.3f; // Giảm độ đậm của gạch ngang
    ImGui::PushStyleColor(ImGuiCol_Separator, sep_col);
    ImGui::Separator();
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
}

void UI_SliderSpeed(const char* label, float* value, float min, float max, float scale, std::function<void(float)> on_change) {
    ImGui::PushID(label);
    
    // Tính toán kích thước dựa trên scale để đồng nhất với giao diện chung
    float slider_height = 4.0f * scale;    // Độ dày thanh trượt
    float grab_size = 7.0f * scale;      // Bán kính nút kéo
    float full_width = ImGui::GetContentRegionAvail().x;

    // Sử dụng helper ModernSliderFloat đã nâng cấp ở trên
    // Chúng ta truyền thêm slider_height và grab_size để nó co giãn theo tỷ lệ video
    if (CSImGui::ModernSliderFloat(
            label,           // Tên hiển thị phía trên slider
            value,           // Giá trị float*
            min,             // Giá trị tối thiểu
            max,             // Giá trị tối đa
            slider_height,   // Chiều cao thanh trượt (Tham số mới)
            grab_size,       // Kích thước nút kéo (Tham số mới)
            "%.2fx",         // Định dạng hiển thị
            full_width,      // Chiều rộng full vùng chứa
            SliderFlags_None
        )) 
    {
        if (on_change) on_change(*value);
    }
    
    ImGui::PopID();
    
    // Khoảng cách đệm phía dưới để không bị dính vào danh sách chọn nhanh
    ImGui::Dummy(ImVec2(0, 15.0f * scale));
}

void UI_SelectableItem(const char* label, bool is_active, float scale, std::function<void()> on_click) {
    ImGui::PushID(label);
    
    float full_width = ImGui::GetContentRegionAvail().x;
    float item_height = 35.0f * scale;
    ImVec2 size = ImVec2(full_width, item_height);

    // 1. Gọi ModernSelectable nâng cao
    if (CSImGui::ModernSelectable("##item_btn", is_active, 0, size)) {
        on_click();
    }

    // Lấy trạng thái animation
    ImGuiID id = ImGui::GetItemID();
    float tHover = ImGui::GetStateStorage()->GetFloat(id, 0.0f);
    float tSelect = ImGui::GetStateStorage()->GetFloat(id + 1, 0.0f);

    bool is_hovered = ImGui::IsItemHovered();
    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    CSImGui::ShowTooltipDelayed(label, is_hovered, 1.5f, label);

    // 2. Vẽ Text với hiệu ứng trượt và màu sắc động
    float slide_offset = tHover * (4.0f * scale);
    float text_max_w = full_width - (45.0f * scale); 
    std::string display_text = TextUtils::TruncateTextByPixels(label, text_max_w);
    
    // Mix màu chữ mượt mà giữa màu thường và màu được chọn
    ImVec4 textColor = CSImGui::GetColors(Col_Text);
    if (tSelect > 0.0f) {
        textColor.x = ImLerp(CSImGui::GetColors(Col_Text).x, CSImGui::GetColors(Col_TextSelected).x, tSelect);
        textColor.y = ImLerp(CSImGui::GetColors(Col_Text).y, CSImGui::GetColors(Col_TextSelected).y, tSelect);
        textColor.z = ImLerp(CSImGui::GetColors(Col_Text).z, CSImGui::GetColors(Col_TextSelected).z, tSelect);
        textColor.w = ImLerp(CSImGui::GetColors(Col_Text).w, CSImGui::GetColors(Col_TextSelected).w, tSelect);
    }

    ImVec2 text_pos = ImVec2(p_min.x + (10.0f * scale) + slide_offset, p_min.y + (item_height - ImGui::GetFontSize()) * 0.5f);
    draw_list->AddText(text_pos, ToIUCol32(textColor), display_text.c_str());

    // 3. Icon Checkmark (Fade in/out theo tSelect)
    if (tSelect > 0.1f) {
        float check_size = (6.0f * scale) * tSelect; // Phóng to dần
        ImVec2 check_pos = ImVec2(p_max.x - 20 * scale, p_min.y + item_height * 0.5f);
        
        ImU32 check_col = ToIUCol32(CSImGui::GetColors(Col_CheckMark));
        // Làm mờ checkmark theo tSelect
        check_col = (check_col & 0x00FFFFFF) | ((uint32_t)(tSelect * 255) << 24);
        
        draw_list->AddCircleFilled(check_pos, check_size * 0.5f, check_col);
    }

    ImGui::PopID();
}

void ResolutionQualityPage(mpv_handle *mpv, VideoAudioFormats &formats, VideoType videotype, float scale) {
    // Tùy chỉnh thanh cuộn (Scrollbar) cho đẹp hơn
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0,0,0,0));

    if (ImGui::BeginChild("##res_scroll_area", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove)) {
        
        for (int i = 0; i < (int)all_formats.video.full_labels.size(); ++i) {
            bool is_active = (all_formats.video_index == i);
            const char* label = all_formats.video.full_labels[i].c_str();

            // Sử dụng Helper mới
            UI_SelectableItem(label, is_active, scale, [&]() {
                if (all_formats.video_index != i) {
                    // --- GIỮ NGUYÊN LOGIC XỬ LÝ MPV CỦA BẠN ---
                    all_formats.video_index = i;
                    all_formats.active_video = all_formats.video.formats[i];
                    v_Settings.selectedFormat = all_formats.video.formats[i];

                    if (g_playbackStatus.hasFile) {
                        if(videotype == VideoType::Live) {
                            const char* cmd1[] = { "set", "ytdl-format", all_formats.video.ids[i].c_str(), nullptr };
                            mpv_command(mpv, cmd1);
                        } else {
                            pendingSeekTime = g_playbackStatus.timePos;
                            v_Settings.selectedResolution = v_Settings.selectedFormat + "+" + v_Settings.selectedAudio;
                            const char* cmd1[] = { "set", "ytdl-format", v_Settings.selectedResolution.c_str(), nullptr };
                            mpv_command(mpv, cmd1);
                        }
                        std::string cmd = "playlist-play-index " + std::to_string(g_playbackStatus.g_PlayingIndex);
                        mpv_command_string(mpv, cmd.c_str());
                    }
                    SaveSettings_Video();
                }
            });

            // Auto focus vào mục đang chọn khi mở menu
            if (is_active && ImGui::IsWindowAppearing()) {
                ImGui::SetScrollHereY();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
void AudioQualityPage( mpv_handle *mpv, VideoAudioFormats &formats , VideoType videotype ,float scale){
    if (ImGui::BeginChild("##audio_scroll_area", ImVec2(0, 0), false)) {
    
        for (int i = 0; i < (int)all_formats.audio.full_labels.size(); ++i) {
            bool is_active = (all_formats.audio_index == i);
            const char* label = all_formats.audio.full_labels[i].c_str();
            
            UI_SelectableItem(label, is_active, scale, [&]() {
                if (all_formats.audio_index != i) {
                    all_formats.audio_index = i;
                    all_formats.active_audio = all_formats.audio.formats[i];
                    v_Settings.selectedAudio = all_formats.audio.formats[i];

                    if (g_playbackStatus.hasFile) {
                        if(videotype == VideoType::Live){
                            const char* cmd1[] = { "set", "ytdl-format", all_formats.audio.ids[i].c_str(), nullptr };
                            mpv_command(mpv, cmd1);
                        }else{
                            pendingSeekTime = g_playbackStatus.timePos;
                            v_Settings.selectedResolution = v_Settings.selectedFormat + "+" + v_Settings.selectedAudio;
                            const char* cmd1[] = { "set", "ytdl-format", v_Settings.selectedResolution.c_str(), nullptr };
                            mpv_command(mpv, cmd1);
                        }
                        
                        std::string cmd = "playlist-play-index " + std::to_string(g_playbackStatus.g_PlayingIndex);
                        mpv_command_string(mpv, cmd.c_str());
                    }
                    SaveSettings_Video();
                }
            });

            // Auto focus vào mục đang chọn khi mở menu
            if (is_active && ImGui::IsWindowAppearing()) {
                ImGui::SetScrollHereY();
            }
        }
    }
    ImGui::EndChild();
}
void PlaybackSpeedPage(mpv_handle *mpv, float scale) {
    // 1. Phần Slider tùy chỉnh (Tự do từ 0.25x đến 4.0x)
    // Tiêu đề nhỏ bên trên thanh trượt
    ImGui::Indent(10 * scale);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Tốc độ tùy chỉnh: %.2fx", v_Settings.playbackSpeed);
    ImGui::Unindent(10 * scale);
    ImGui::Dummy(ImVec2(0, 5 * scale));

    // Gọi thanh trượt Full-width
    float current_speed = (float)v_Settings.playbackSpeed;
    
    UI_SliderSpeed("SpeedSlider", &current_speed, 0.25f, 4.0f, scale, [&](float new_speed) {
        v_Settings.playbackSpeed = (double)new_speed;
        // Gửi lệnh trực tiếp đến mpv
        mpv_command_set_speed( mpv, (double)new_speed);
        SaveSettings_Video();
    });

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10 * scale));
    
    // Tiêu đề nhỏ cho danh sách chọn nhanh
    ImGui::Indent(10 * scale);
    ImGui::TextDisabled("CHỌN NHANH");
    ImGui::Unindent(10 * scale);
    ImGui::Dummy(ImVec2(0, 5 * scale));

    // 2. Danh sách các mốc cố định (Sử dụng UI_SelectableItem bạn đã có)
    static const float speeds[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f };
    
    if (ImGui::BeginChild("##speed_list", ImVec2(0, 0), false)) {
        for (float s : speeds) {
            char buf[16]; 
            snprintf(buf, sizeof(buf), "%.2fx", s);
            
            // So sánh gần đúng để highlight mục đang chọn
            bool is_active = (fabs(v_Settings.playbackSpeed - s) < 0.01f);
            
            UI_SelectableItem(buf, is_active, scale, [&]() {
                v_Settings.playbackSpeed = (double)s;
                mpv_command_set_speed(mpv, (double)s);
                SaveSettings_Video();
            });

            if (is_active && ImGui::IsWindowAppearing()) {
                ImGui::SetScrollHereY();
            }
        }
    }
    ImGui::EndChild();
}
void OptionsPage(){

}
void RenderIOCHSidebar(mpv_handle * mpv, ImVec2 videoPos, ImVec2 videoSize, bool open, bool& show_ui_video ,ImVec2 iconPos) {
    static float anim = 0.0f;
    UpdateHoverAnim(anim, open, 15.0f);
    
    if (anim < 0.01f) {
        current_page = SettingsPage::Main; 
        return;
    }

    float scaleFactor = videoSize.y / 720.0f;

    scaleFactor = std::max(scaleFactor, 1.0f);
    scaleFactor = std::min(scaleFactor, 2.0f);

    // Tính toán kích thước mục tiêu dựa trên trang hiện tại
    float targetWidth = 260.0f * scaleFactor;
    float targetHeight = 350.0f * scaleFactor;

    if (current_page == SettingsPage::ResolutionQuality || 
        current_page == SettingsPage::AudioQuality ) {
        targetWidth = 300.0f * scaleFactor;  // Phình rộng thêm một chút cho rõ chữ
        targetHeight = 370.0f * scaleFactor; // Phình to chiều cao cho danh sách dài
    }
    ImVec2 size_target = ImVec2(targetWidth,targetHeight);
    // Nội suy kích thước cửa sổ để có hiệu ứng co giãn mượt mà
    static ImVec2 current_window_size = ImVec2(targetWidth, targetHeight);
    current_window_size = ImLerp(current_window_size, size_target, SMOOTH_LERP( 15.0f,ImGui::GetIO().DeltaTime));

    ImVec2 windowSize(current_window_size);

    //GIỚI HẠN KÍCH THƯỚC (Không được to hơn video trừ đi padding)
    float padding = 30.0f * scaleFactor;
    float maxAllowedW = videoSize.x - (padding * 2.0f);
    float maxAllowedH = videoSize.y - (padding * 2.0f);

    // Nếu windowSize hiện tại (đang nội suy) to hơn mức cho phép, hãy ép nó lại
    if (windowSize.x > maxAllowedW) windowSize.x = maxAllowedW;
    if (windowSize.y > maxAllowedH) windowSize.y = maxAllowedH;

    // Cập nhật lại biến nội suy để không bị "đấu đá" với logic phình to
    current_window_size.x = ImMin(current_window_size.x, maxAllowedW);
    current_window_size.y = ImMin(current_window_size.y, maxAllowedH);

    // Tính toán vị trí lý tưởng (Mặc định: Phía trên Icon, sát lề phải)
    ImVec2 windowPos(
        videoPos.x + videoSize.x - windowSize.x - padding, // X: Sát mép phải video (có padding)
        iconPos.y - windowSize.y - padding                 // Y: Ngay phía trên Icon
    );

    // 2. Logic ưu tiên: Nếu tràn đỉnh video, đẩy xuống dưới Icon
    if (windowPos.y < videoPos.y) {
        windowPos.y = iconPos.y + 40.0f * scaleFactor; 
    }

    // 3. --- GIỚI HẠN BIÊN (BOUNDARIES CHECK) ---
    // Đảm bảo không vượt quá giới hạn trái/phải của Video
    float minX = videoPos.x + padding;
    float maxX = videoPos.x + videoSize.x - windowSize.x - padding;
    windowPos.x = ImClamp(windowPos.x, minX, maxX);

    // Đảm bảo không vượt quá giới hạn trên/dưới của Video
    float minY = videoPos.y + padding;
    float maxY = videoPos.y + videoSize.y - windowSize.y - padding;

    windowPos.y = ImClamp(windowPos.y, minY, maxY);

    // Sau khi đã Clamp windowPos.y ở bước 3 của bạn:
    ImRect iconRect(iconPos, iconPos + ImVec2(40 * scaleFactor, 40 * scaleFactor));
    ImRect windowRect(windowPos, windowPos + windowSize);

    if (windowRect.Overlaps(iconRect)) {
        // Nếu đè lên icon, đẩy sidebar sang trái icon 1 chút
        windowPos.x = iconPos.x - windowSize.x - padding;
        // Và tiếp tục Clamp X lại để không lòi ra khỏi lề trái video
        windowPos.x = ImClamp(windowPos.x, videoPos.x + padding, maxX);
    }

    // Thiết lập cửa sổ
    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(windowSize);
    ImGui::SetNextWindowBgAlpha(0.92f * anim);

    // Style cho Window
    CSImGui::PushModernWindowStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim);

    if (ImGui::Begin("##SettingsSidebar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        // --- HEADER ---
        // Cập nhật tiến trình animation trang (0.0 -> 1.0)
        page_anim = ImMin(page_anim + ImGui::GetIO().DeltaTime * 6.0f, 1.0f);

        // Hiệu ứng Slide: Trang mới sẽ trượt từ dưới lên hoặc từ phải sang
        float slide_up = (1.0f - page_anim) * 20.0f * scaleFactor;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + slide_up);

        // Áp dụng Alpha cho nội dung trang
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim * page_anim);

        VideoType videotype = GetVideoType();
        // --- NỘI DUNG ---
        switch (current_page) {
            case SettingsPage::Main:
            {
                UI_GroupHeader("Chất lượng",scaleFactor);
                if (videotype != VideoType::Local) {
                    const char* res_label = all_formats.video.short_labels.empty() ? "N/A" : all_formats.video.short_labels[all_formats.video_index].c_str();
                    UI_MenuItem("Độ phân giải", res_label,scaleFactor, [&]() { ChangePage(SettingsPage::ResolutionQuality); });
                }
                if (videotype != VideoType::Local) {
                    const char* res_label = all_formats.audio.short_labels.empty() ? "N/A" : all_formats.audio.short_labels[all_formats.audio_index].c_str();
                    UI_MenuItem("Chất lượng âm thanh", res_label, scaleFactor, [&]() { ChangePage(SettingsPage::AudioQuality); });
                }
    
                char speed_buf[16];
                snprintf(speed_buf, sizeof(speed_buf), "%.2fx", v_Settings.playbackSpeed);
                UI_MenuItem("Tốc độ phát", speed_buf, scaleFactor, [&]() { ChangePage(SettingsPage::PlaybackSpeed); });

                UI_MenuItem("Tùy chọn nâng cao", "Thiết lập", scaleFactor, [&]() { ChangePage(SettingsPage::Options); });

                ImGui::Spacing();
                UI_GroupHeader("Tùy chọn",scaleFactor);

                UI_Toggle("Phụ đề", &v_Settings.enableSubtitles, scaleFactor, g_videoInfo.hasSubtitles, [&](bool s) {
                    mpv_set_property_string(mpv, "sub-visibility", s ? "yes" : "no");
                    mpv_set_property_string(mpv, "sid", "1");
                    SaveSettings_Video();
                });

                UI_Toggle("Lặp lại video", &v_Settings.repeatVideo, scaleFactor, true, [&](bool s) {
                    mpv_set_property_string(mpv, "loop-file", s ? "inf" : "no");
                    SaveSettings_Video();
                });

                UI_Toggle("Tự động phát tiếp", &v_Settings.autoPlayNext, scaleFactor, true, [&](bool s) {
                    mpv_set_property_string(mpv, "playlist-auto-advance", s ? "yes" : "no");
                    SaveSettings_Video();
                });

                UI_Toggle("Trình chiếu âm thanh ", &Audio_visualizers, scaleFactor, true, [&](bool s) {
                });
                break;
            }
        
            // --- TRANG CON: RESOLUTION ---
            case SettingsPage::ResolutionQuality:
            {
                if (CSImGui::ModernSelectable("< Quay lại", false, 0, ImVec2(0, 25))) ChangePage(SettingsPage::Main);
                ImGui::Separator();
                ImGui::Spacing();

                ResolutionQualityPage(mpv, all_formats, videotype , scaleFactor);
                break;
            }

            case SettingsPage::AudioQuality:
            {
                if (CSImGui::ModernSelectable("< Quay lại", false, 0, ImVec2(0, 25))) ChangePage(SettingsPage::Main);
                ImGui::Separator();
                ImGui::Spacing();

                AudioQualityPage(mpv, all_formats, videotype , scaleFactor);
                break;
            }
        
            // --- TRANG CHỌN TỐC ĐỘ ---
            case SettingsPage::PlaybackSpeed:
            {
                if (CSImGui::ModernSelectable("< Quay lại", false, 0, ImVec2(0, 25))) ChangePage(SettingsPage::Main);
                ImGui::Separator();
                ImGui::Spacing();

                PlaybackSpeedPage(mpv , scaleFactor);
                break;
            }
            case SettingsPage::Options:
            {
                switch (current_options_page)
                {
                    case OptionsPage::Main:
                    {
                        if (CSImGui::ModernSelectable("< Quay lại", false, 0, ImVec2(0, 25))) ChangePage(SettingsPage::Main);
                        ImGui::Separator();
                        ImGui::Spacing();

                        UI_MenuItem("Tùy chọn Phụ đề", "Cài đặt phụ đề", scaleFactor, [&]() { current_options_page = OptionsPage::Subtitles; });
                        break;
                    }
                    case OptionsPage::Subtitles:
                    {
                        if (CSImGui::ModernSelectable("< Quay lại", false, 0, ImVec2(0, 25))) current_options_page = OptionsPage::Main;
                        ImGui::Separator();
                        ImGui::Spacing();

                        UI_Toggle("Bật phụ đề", &v_Settings.enableSubtitles, scaleFactor, g_videoInfo.hasSubtitles, [&](bool s) {
                            mpv_set_property_string(mpv, "sub-visibility", s ? "yes" : "no");
                            mpv_set_property_string(mpv, "sid", "1");
                            SaveSettings_Video();
                        });

                        break;
                    }
                }

               


                break;
            }
        }
        if (ImGui::IsAnyItemActive()) NotifyActivity(show_ui_video);
        ImGui::PopStyleVar();
        ImGui::End();
    }
    CSImGui::PopModernWindowStyle();
    ImGui::PopStyleVar();
}


void ApplyPlaybackSettings(mpv_handle * mpv) {
    if (!mpv) return;

    // 1. Thiết lập các thông số dạng số (Double)
    int volume = v_Settings.defaultVolume;
    mpv_command_set_volume(mpv, volume);

    double speed = (double)v_Settings.playbackSpeed;
    mpv_command_set_speed(mpv, speed);

    double audiodelay = (double)v_Settings.audiodelay;
    mpv_command_set_audio_delay(mpv, audiodelay);

    // 2. Thiết lập Subtitles (Dùng mpv_set_property_string cho gọn và an toàn)
    const char* sub_vis = v_Settings.enableSubtitles ? "yes" : "no"; // mpv dùng "no" để tắt sub
    mpv_set_property_string(mpv, "sub-visibility", sub_vis);
    if(v_Settings.enableSubtitles){
        mpv_set_property_string(mpv, "sid", "1");
    }
    

    // 3. Thiết lập Repeat File (loop-file)
    const char* repeat_mode = v_Settings.repeatVideo ? "inf" : "no";
    mpv_set_property_string(mpv, "loop-file", repeat_mode);

    // 4. Thiết lập Auto Play Next
    const char* auto_next_mode = v_Settings.autoPlayNext ? "yes" : "no";
    mpv_set_property_string(mpv, "playlist-auto-advance", auto_next_mode);

    // 5. Thiết lập Loop Playlist
    const char* loop_list = v_Settings.repeatlist ? "force" : "no";
    // Sửa lỗi: Truyền đúng biến loop_list_cmd hoặc dùng set_property_string
    mpv_set_property_string(mpv, "loop-playlist", loop_list);
}


int PlayVideo(mpv_handle * mpv,
              const std::string& Url, 
              const std::string& resolutionFormat, 
              const std::string& title )
{
    if (!mpv)
        return -1;
    std::string chosenFormat = resolutionFormat;
    if (!(GetVideoType() == VideoType::Local))
    {
        // Cấu hình cookie
        const char* cmdCookie[] = { "set", "ytdl-cookie", "temp/cookies.txt", nullptr };
        mpv_command(mpv, cmdCookie);

        // Set format nếu có
        if (!chosenFormat.empty()) {
            const char* cmd1[] = { "set", "ytdl-format", chosenFormat.c_str(), nullptr };
            int res1 = mpv_command(mpv, cmd1);
            if (res1 < 0)
                return res1;
        }
    }
    
    const char* cmd2[] = { "loadfile", Url.c_str(), "append-play", nullptr };
    int res2 = mpv_command(mpv, cmd2);
    if (res2 < 0)
        return res2;

    ApplyPlaybackSettings(mpv);
    return 0;
}

void CallThread_URLFetch(const std::string& Url , bool playNow ,  const std::string& title ,const std::string& format_id) {

    GetThreadManager().Run(ThreadID::URLFetch, [=]() {

        playImmediately = playNow;  
        v_Settings.selectedResolution = v_Settings.selectedFormat + "+" + v_Settings.selectedAudio;
        int result = PlayVideo(mpv.mpv, Url, (!format_id.empty() ? format_id : v_Settings.selectedResolution ), title);
    });
}
