#include "mpv/mpv_settings.h"   
#include "mpv/mpv_ui_settings.h"
#include "mpv/mpv_controller.h"
#include "mpv/mpv_basic_formats.h"
#include "mpv/mpv_ui.h"

#include "globals.h"
#include "utils.h"
#include "json.hpp"


#include <imgui.h>
//#undef RATE_LIMITED_COUT
//#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>
#include <string>

enum class SettingsPage { Main, ResolutionQuality, AudioQuality, PlaybackSpeed ,Options };
enum class OptionsPage { Main, Subtitles };
static SettingsPage current_page = SettingsPage::Main;
static OptionsPage current_options_page = OptionsPage::Main;


// Thêm màu sắc và khoảng cách chuẩn
void UI_MenuItem(const char* label, const char* current_value, float scale, std::function<void()> on_click) {
    ImGui::PushID(label);
    
    // 1. Tính toán kích thước dựa trên scale
    float full_width = ImGui::GetContentRegionAvail().x;
    float item_height = 40.0f * scale; // Chiều cao dòng co giãn theo video
    ImVec2 size = ImVec2(full_width, item_height); 
    
    // Tạo vùng tương tác
    if (ImGui::Selectable("##item", false, 0, size)) {
        on_click();
    }
    
    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Hiệu ứng Hover
    if (ImGui::IsItemHovered()) {
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 15), 4.0f * scale);
    }

    // 2. Thiết lập "Bức tường ảo" (Ví dụ: Label chiếm 55%, Value chiếm 45%)
    float margin_left = 12.0f * scale;
    float arrow_space = 35.0f * scale;
    float center_split = full_width * 0.55f; // Điểm chia giữa 2 vùng text

    // --- XỬ LÝ VÀ VẼ LABEL (Bên trái) ---
    float max_label_w = center_split - margin_left - (5.0f * scale);
    std::string safe_label = TextUtils::TruncateToWidth(label, max_label_w);
    
    draw_list->AddText(ImVec2(p_min.x + margin_left, p_min.y + (item_height * 0.25f)), 
                       IM_COL32(240, 240, 240, 255), safe_label.c_str());

    // --- XỬ LÝ VÀ VẼ VALUE (Bên phải) ---
    if (current_value && strlen(current_value) > 0) {
        // Vùng khả dụng cho value nằm giữa center_split và arrow_space
        float max_val_w = (full_width - center_split) - arrow_space - (5.0f * scale);
        
        std::string safe_value = TextUtils::TruncateToWidth(current_value, max_val_w);
        float val_text_width = ImGui::CalcTextSize(safe_value.c_str()).x;
        
        // Căn lề phải: p_max.x trừ đi khoảng cách mũi tên và độ rộng chữ
        draw_list->AddText(ImVec2(p_max.x - val_text_width - arrow_space, p_min.y + (item_height * 0.25f)), 
                           IM_COL32(160, 160, 160, 255), safe_value.c_str());
    }
    
    // --- VẼ MŨI TÊN ĐIỀU HƯỚNG ---
    draw_list->AddText(ImVec2(p_max.x - (20.0f * scale), p_min.y + (item_height * 0.25f)), 
                       IM_COL32(100, 100, 100, 255), ">");
    
    // Spacing cũng phải scale để không bị dính cục khi thu nhỏ
    ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));
    ImGui::PopID();

}

void UI_Toggle(const char* label, bool* v, float scale, bool enabled , std::function<void(bool)> on_change) {
    ImGui::PushID(label);
    
    // --- QUẢN LÝ ANIMATION ---
    static std::map<ImGuiID, float> anim_states;
    ImGuiID id = ImGui::GetID(label);
    if (anim_states.find(id) == anim_states.end()) anim_states[id] = (*v ? 1.0f : 0.0f);

    // Tốc độ mượt (0.1f - 0.2f tùy sở thích)
    float target = *v ? 1.0f : 0.0f;
    anim_states[id] += (target - anim_states[id]) * 0.15f; 
    float t = anim_states[id]; // t chạy từ 0 -> 1

    // --- THIẾT LẬP KÍCH THƯỚC SCALE ---
    float width = ImGui::GetContentRegionAvail().x;
    float row_height = 35.0f * scale;
    float sw_w = 36.0f * scale;
    float sw_h = 18.0f * scale;
    float knob_r = 7.0f * scale;

    // Thiết lập độ trong suốt dựa trên trạng thái enabled
    // Nếu disabled, dùng alpha thấp (vd: 100), nếu enabled dùng full alpha (255)
    float alpha_mult = enabled ? 1.0f : 0.4f; 
    ImU32 text_col = IM_COL32(240, 240, 240, (int)(255 * alpha_mult));
    ImU32 knob_col = IM_COL32(255, 255, 255, (int)(255 * alpha_mult));

    if (ImGui::InvisibleButton("##toggle_btn", ImVec2(width, row_height)) && enabled) {
        *v = !*v;
        if (on_change) on_change(*v);
    }

    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // 1. Background khi hover
    if ( enabled && ImGui::IsItemHovered()) {
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 10), 4.0f * scale);
    }

    // 2. Label (Sử dụng TruncateTextByPixels để tránh đè lên switch)
    float max_label_w = width - sw_w - (30.0f * scale);
    std::string safe_label = TextUtils::TruncateTextByPixels(label, max_label_w);
    draw_list->AddText(ImVec2(p_min.x + 12 * scale, p_min.y + (row_height - ImGui::GetFontSize()) * 0.5f), 
                       text_col, safe_label.c_str());

    // 3. Switch Background (Nội suy màu sắc - Interpolation)
    ImVec2 sw_pos = ImVec2(p_max.x - sw_w - 12 * scale, p_min.y + (row_height - sw_h) * 0.5f);
    
    // Màu OFF (70,70,70) -> Màu ON (50,205,50)
    int r, g, b;

    if (enabled) {
        r = (int)(70 + (50 - 70) * t);
        g = (int)(70 + (205 - 70) * t);
        b = (int)(70 + (50 - 70) * t);
    } else {
        // Khi disable, ta cho nó một màu xám tối và nhạt hơn
        r = g = b = 45; 
    }

    ImU32 col_bg = IM_COL32(r, g, b, 255);

    draw_list->AddRectFilled(sw_pos, ImVec2(sw_pos.x + sw_w, sw_pos.y + sw_h), col_bg, 10.0f * scale);

    // 4. Knob (Nội suy vị trí)
    // t = 0: sát lề trái, t = 1: sát lề phải
    float start_x = sw_pos.x + (sw_h * 0.5f);
    float end_x = sw_pos.x + sw_w - (sw_h * 0.5f);
    float knob_x = start_x + (end_x - start_x) * t;

    draw_list->AddCircleFilled(ImVec2(knob_x, sw_pos.y + sw_h * 0.5f), knob_r, knob_col);

    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, 2.0f * scale)); // Khoảng cách nhỏ giữa các dòng
}

void UI_GroupHeader(const char* title) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::Text(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();
}

void UI_SliderSpeed(const char* label, float* value, float min, float max, float scale, std::function<void(float)> on_change) {
    ImGui::PushID(label);
    
    // 1. Lấy toàn bộ chiều rộng khả dụng
    float width = ImGui::GetContentRegionAvail().x;
    
    // 2. Thiết lập Style "Mảnh" (Slim)
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 2.0f * scale)); // Hẹp lại (giảm từ 5 xuống 2)
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 12.0f * scale);          // Nút kéo nhỏ lại cho tinh tế
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 12.0f * scale);

    // 3. Màu sắc Modern
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(45, 45, 48, 255));          // Nền thanh trượt tối
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(55, 55, 58, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(65, 65, 68, 255));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, IM_COL32(50, 205, 50, 255));      // Nút kéo màu xanh lá
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, IM_COL32(80, 255, 80, 255));

    // Đặt chiều rộng cho item tiếp theo (Slider)
    ImGui::PushItemWidth(width); 
    
    // Vẽ Slider không có Label bên cạnh (dùng ## để ẩn label mặc định của ImGui)
    if (ImGui::SliderFloat("##speed_slider", value, min, max, "%.2fx")) {
        if (on_change) on_change(*value);
    }
    
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(4);
    
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0, 10.0f * scale)); // Khoảng cách dưới
}

void UI_SelectableItem(const char* label, bool is_active, float scale, std::function<void()> on_click) {
    ImGui::PushID(label);
    
    float full_width = ImGui::GetContentRegionAvail().x;
    float item_height = 35.0f * scale; // Scale theo video
    ImVec2 size = ImVec2(full_width, item_height);

    // Bắt đầu một Group để xử lý click và hover
    ImGui::BeginGroup();
    
    // InvisibleButton để bắt tương tác
    if (ImGui::InvisibleButton("##item_btn", size)) {
        on_click();
    }

    bool is_hovered = ImGui::IsItemHovered();
    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // --- VẼ BACKGROUND ---
    if (is_active) {
        // Màu khi đang được chọn (Xanh nhẹ)
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(50, 205, 50, 40), 4.0f * scale);
    } else if (is_hovered) {
        // Màu khi hover (Trắng mờ)
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 15), 4.0f * scale);
    }

    ShowTooltipDelayed(label, is_hovered, 1.5f, label); // Hiển thị tooltip sau 0.5s hover

    // --- VẼ TEXT (Cắt chữ nếu quá dài) ---
    float text_max_w = full_width - (40.0f * scale); // Trừa chỗ cho icon check
    std::string display_text = TextUtils::TruncateTextByPixels(label, text_max_w);
    
    ImVec2 text_pos = ImVec2(p_min.x + 10 * scale, p_min.y + (item_height - ImGui::GetFontSize()) * 0.5f);
    draw_list->AddText(text_pos, is_active ? IM_COL32(50, 255, 150, 255) : IM_COL32(230, 230, 230, 255), display_text.c_str());

    // --- VẼ ICON CHECKMARK (Nếu active) ---
    if (is_active) {
        float check_size = 6.0f * scale;
        ImVec2 check_pos = ImVec2(p_max.x - 20 * scale, p_min.y + item_height * 0.5f);
        // Vẽ dấu chấm tròn hoặc icon check nhỏ
        draw_list->AddCircleFilled(check_pos, check_size * 0.5f, IM_COL32(50, 255, 150, 255));
    }

    ImGui::EndGroup();
    ImGui::PopID();
    
    // Tạo khoảng cách giữa các dòng
    ImGui::Dummy(ImVec2(0, 0));
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

    ImVec2 windowSize(260 * scaleFactor, 350 * scaleFactor);

    float padding = 30.0f * scaleFactor;
    ImVec2 windowPos(

        videoPos.x + videoSize.x - windowSize.x - padding, // X: Sát mép phải video (có padding)
        iconPos.y - windowSize.y - padding                 // Y: Ngay phía trên Icon
    );

    // Nếu Sidebar bị tràn khỏi đỉnh video, đẩy nó xuống dưới Icon
    if (windowPos.y < videoPos.y) {
        windowPos.y = iconPos.y + 40 * scaleFactor; 
    }

    // Thiết lập cửa sổ

    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(windowSize);
    ImGui::SetNextWindowBgAlpha(0.92f * anim);

    // Style cho Window
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));

    if (ImGui::Begin("##SettingsSidebar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        
        // --- HEADER ---

        VideoType videotype = GetVideoType();

        // --- NỘI DUNG ---
        switch (current_page) {
            case SettingsPage::Main:
            {
                UI_GroupHeader("Chất lượng");
                if (videotype != VideoType::File_Local) {
                    const char* res_label = all_formats.video.short_labels.empty() ? "N/A" : all_formats.video.short_labels[all_formats.video_index].c_str();
                    UI_MenuItem("Độ phân giải", res_label,scaleFactor, [&]() { current_page = SettingsPage::ResolutionQuality; });
                }
                if (videotype != VideoType::File_Local) {
                    const char* res_label = all_formats.audio.short_labels.empty() ? "N/A" : all_formats.audio.short_labels[all_formats.audio_index].c_str();
                    UI_MenuItem("Chất lượng âm thanh", res_label, scaleFactor, [&]() { current_page = SettingsPage::AudioQuality; });
                }
    
                char speed_buf[16];
                snprintf(speed_buf, sizeof(speed_buf), "%.2fx", v_Settings.playbackSpeed);
                UI_MenuItem("Tốc độ phát", speed_buf, scaleFactor, [&]() { current_page = SettingsPage::PlaybackSpeed; });

                UI_MenuItem("Tùy chọn nâng cao", "Thiết lập", scaleFactor, [&]() { current_page = SettingsPage::Options; });

                ImGui::Spacing();
                UI_GroupHeader("Tùy chọn");

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
                break;
            }
        
            // --- TRANG CON: RESOLUTION ---
            case SettingsPage::ResolutionQuality:
            {
                if (ImGui::Selectable("< Quay lại", false, 0, ImVec2(0, 25))) current_page = SettingsPage::Main;
                ImGui::Separator();
                ImGui::Spacing();

                ResolutionQualityPage(mpv, all_formats, videotype , scaleFactor);
                break;
            }

            case SettingsPage::AudioQuality:
            {
                if (ImGui::Selectable("< Quay lại", false, 0, ImVec2(0, 25))) current_page = SettingsPage::Main;
                ImGui::Separator();
                ImGui::Spacing();

                AudioQualityPage(mpv, all_formats, videotype , scaleFactor);
                break;
            }
        
            // --- TRANG CHỌN TỐC ĐỘ ---
            case SettingsPage::PlaybackSpeed:
            {
                if (ImGui::Selectable("< Quay lại", false, 0, ImVec2(0, 25))) current_page = SettingsPage::Main;
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
                        if (ImGui::Selectable("< Quay lại", false, 0, ImVec2(0, 25))) current_page = SettingsPage::Main;
                        ImGui::Separator();
                        ImGui::Spacing();

                        UI_MenuItem("Tùy chọn Phụ đề", "Cài đặt phụ đề", scaleFactor, [&]() { current_options_page = OptionsPage::Subtitles; });
                        break;
                    }
                    case OptionsPage::Subtitles:
                    {
                        if (ImGui::Selectable("< Quay lại", false, 0, ImVec2(0, 25))) current_options_page = OptionsPage::Main;
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
    ImGui::End();
    ImGui::PopStyleVar(3);
    }
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
    if (!(GetVideoType() == VideoType::File_Local))
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


