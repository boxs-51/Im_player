#include "mpv/mpv_settings.h"   
#include "mpv/mpv_ui_settings.h"
#include "mpv/mpv_controller.h"
#include "mpv/mpv_basic_formats.h"

#include "globals.h"
#include "utils.h"
#include "json.hpp"

#include <imgui.h>
//#undef RATE_LIMITED_COUT
//#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>
#include <string>


static std::string url_play = "";


static bool is_live = false;
static bool file_local = false;

// Vị trí lưu file cấu hình
bool ShowAudioFormatCombo(mpv_handle *mpv, VideoAudioFormats &formats) {
    if (formats.audio.labels.empty()) return false;

    int current_index = formats.audio_index >= 0 ? formats.audio_index : 0;
    const char* current_label = formats.audio.labels[current_index].c_str();

    if (ImGui::BeginCombo("##audio_format_combo", current_label)) {
        for (int i = 0; i < formats.audio.labels.size(); ++i) {
            bool selected = (i == current_index);

            // Khi chọn lại format đang dùng, không đóng combo
            ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
            if (selected)
                flags |= ImGuiSelectableFlags_DontClosePopups;

            if (ImGui::Selectable(formats.audio.labels[i].c_str(), selected , flags)) {
                // Cập nhật chỉ khi khác
                if (i < formats.audio.formats.size() && i != formats.audio_index) {
                    formats.audio_index = i;
                    formats.active_audio = formats.audio.formats[i];
                    v_Settings.selectedAudio = all_formats.audio.formats[i];

                    if (g_playbackStatus.hasFile) {
                        if(is_live){
                            const char* cmd1[] = { "set", "ytdl-format", all_formats.audio.ids[i].c_str(), nullptr };
                            int res = mpv_command(mpv, cmd1);
                            RATE_LIMITED_COUT(mpv_audio_format_change_live, 1,std::cout << "[DEBUG] [INFO] [MPV] Audio format changed for live stream to " << all_formats.audio.ids[i] << "");
                        }else{
                            pendingSeekTime = g_playbackStatus.timePos;
                            v_Settings.selectedResolution = v_Settings.selectedFormat + "+" + v_Settings.selectedAudio;
                            const char* cmd1[] = { "set", "ytdl-format", v_Settings.selectedResolution .c_str(), nullptr };
                            int res = mpv_command(mpv, cmd1);
                            RATE_LIMITED_COUT(mpv_audio_format_change, 1,std::cout << "[DEBUG] [INFO] [MPV] Audio format changed to " << v_Settings.selectedAudio << "");
                        }
                        std::string cmd = "playlist-play-index " + std::to_string(g_playbackStatus.g_PlayingIndex);
                        int res = mpv_command_string(mpv, cmd.c_str());
                        RATE_LIMITED_COUT(mpv_audio_format_play_index, 1,std::cout << "[DEBUG] [INFO] [MPV] Playing index " << g_playbackStatus.g_PlayingIndex << " after audio format change.");
                    }
                    SaveSettings_Video();
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return true;
}

bool ShowVideoFormatCombo(mpv_handle *mpv, VideoAudioFormats &formats) {
    if (formats.video.labels.empty()) return false;

    int current_index = formats.video_index >= 0 ? formats.video_index : 0;
    const char* current_label = formats.video.labels[current_index].c_str();

    if (ImGui::BeginCombo("##resolution_combo", current_label)) {
        for (int i = 0; i < formats.video.labels.size(); ++i) {
            bool selected = (formats.video_index == i);
            // Khi chọn lại format đang dùng, không đóng combo
            ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
            if (selected)
                flags |= ImGuiSelectableFlags_DontClosePopups;
            if (ImGui::Selectable(formats.video.labels[i].c_str(), selected, flags)) {
                // Cập nhật chỉ khi khác
                if (i < formats.video.formats.size() && i != formats.video_index) {
                    formats.video_index = i;
                    formats.active_video = formats.video.formats[i];
                    v_Settings.selectedFormat = formats.video.formats[i];
                    //if (!url_play.empty()) {
                    if (g_playbackStatus.hasFile) {

                        if(is_live){
                            const char* cmd1[] = { "set", "ytdl-format", all_formats.video.ids[i].c_str(), nullptr };
                            int res = mpv_command(mpv, cmd1);
                            RATE_LIMITED_COUT(mpv_video_format_change_live, 1,std::cout << "[DEBUG] [INFO] [MPV] Video format changed for live stream to " << all_formats.video.ids[i] << "");
                        }else{
                            pendingSeekTime = g_playbackStatus.timePos;
                            v_Settings.selectedResolution = v_Settings.selectedFormat + "+" + v_Settings.selectedAudio;
                            const char* cmd1[] = { "set", "ytdl-format", v_Settings.selectedResolution .c_str(), nullptr };
                            int res = mpv_command(mpv, cmd1);
                            RATE_LIMITED_COUT(mpv_video_format_change, 1,std::cout << "[DEBUG] [INFO] [MPV] Video format changed to " << v_Settings.selectedFormat << "");
                        }
                        std::string cmd = "playlist-play-index " + std::to_string(g_playbackStatus.g_PlayingIndex);
                        int res = mpv_command_string(mpv, cmd.c_str());
                        RATE_LIMITED_COUT(mpv_video_format_play_index, 1,std::cout << "[DEBUG] [INFO] [MPV] Playing index " << g_playbackStatus.g_PlayingIndex << " after video format change.");
                    }
                    SaveSettings_Video();
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return true;
}
bool RenderToggleCombo(const char* label, bool& state) {
    const char* options[] = { "Tắt", "Bật" };
    int current_index = state ? 1 : 0;
    bool changed = false;

    if (ImGui::BeginCombo(label, options[current_index])) {
        for (int i = 0; i < 2; ++i) {
            bool is_selected = (current_index == i);
            if (ImGui::Selectable(options[i], is_selected)) {
                if (current_index != i) {
                    current_index = i;
                    state = (i == 1);
                    changed = true;
                }
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    return changed;
}

void RenderIOCHSidebar(mpv_handle * mpv ,ImVec2 videoPos, ImVec2 videoSize , bool open) {
    
    static float anim = 0.0f;
    UpdateHoverAnim(anim ,open, 2.0f);
    // Nếu panel gần như tắt hoàn toàn thì không cần vẽ
    if (anim < 0.01f )
        return;

    ImVec2 windowSize(250, 300);
    ImVec2 windowPos(
        videoPos.x + videoSize.x * 0.98f - windowSize.x,
        videoPos.y + videoSize.y * 0.85f - windowSize.y
    );
    // ---- Set window trước Begin ----
    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(windowSize);
    ImGui::SetNextWindowBgAlpha(0.9f * anim);

    // ---- Style ----
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.5f, 1.5f)); // mỏng
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0, 0, 0, 0));   // nền trong suốt
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1, 1, 1, 0.1f)); // hover nhẹ
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(1, 1, 1, 0.2f)); // click sáng hơn
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove ;
    if (ImGui::Begin("##SettingsSidebar",nullptr,window_flags)) {
        ImGui::Text("Cấu hình phát video");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTable("SettingsTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

            // --- Độ phân giải ---
            if  (!file_local && all_formats.video.labels.size() >=0){
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Độ phân giải:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                ShowVideoFormatCombo(mpv, all_formats);
                ImGui::PopItemWidth();
            }

            if  (!is_live && !file_local && all_formats.audio.labels.size() >= 0){
                // --- Chất lượng audio ---
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Chất lượng audio:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                ShowAudioFormatCombo(mpv, all_formats);
                ImGui::PopItemWidth();
            }

            // --- Phụ đề ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Phụ đề:");
            ImGui::TableSetColumnIndex(1);
            
            ImGui::PushItemWidth(-1);
            if (RenderToggleCombo("##Subtitles_video_combo", v_Settings.enableSubtitles)) {
                if (mpv) {
                    const char* value = v_Settings.enableSubtitles ? "auto" : "0";
                    const char* cmd[] = { "set", "sid", value, nullptr };
                    mpv_command(mpv, cmd);
                }
                SaveSettings_Video();
            }
            ImGui::PopItemWidth();

            // --- Tốc độ phát ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Tốc độ phát:");
            ImGui::TableSetColumnIndex(1);

            static const float speedPresets[] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f };
            static const char* speedLabels[] = { "0.25x", "0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x", "3.0x" };

            // Tìm preset khớp
            int presetIndex = -1;
            for (int i = 0; i < IM_ARRAYSIZE(speedPresets); i++) {
                if (fabs(v_Settings.playbackSpeed - speedPresets[i]) < 0.001f) {
                    presetIndex = i;
                    break;
                }
            }

            // Tạo nhãn động
            char comboLabel[16];
            if (presetIndex >= 0) {
                snprintf(comboLabel, sizeof(comboLabel), "%s", speedLabels[presetIndex]); // preset khớp
            } else {
                snprintf(comboLabel, sizeof(comboLabel), "%.2fx", v_Settings.playbackSpeed); // giá trị lẻ
            }

            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##speed_combo", comboLabel)) {

                float oldSpeed = v_Settings.playbackSpeed;

                ImGui::Text("Tùy chỉnh:");
                if (ImGui::SliderFloat("##speed_slider", &v_Settings.playbackSpeed, 0.25f, 3.0f, "%.2fx")) {
                    double speed = static_cast<double>(v_Settings.playbackSpeed);
                    mpv_command_set_speed(mpv,speed);
                    SaveSettings_Video();
                }

                ImGui::Separator();

                // Danh sách preset tốc độ
                for (int i = 0; i < IM_ARRAYSIZE(speedPresets); ++i) {
                    bool selected = (presetIndex == i);
                    if (ImGui::Selectable(speedLabels[i], selected)) {
                        v_Settings.playbackSpeed = speedPresets[i];
                        double speed = static_cast<double>(v_Settings.playbackSpeed);
                        mpv_command_set_speed(mpv,speed);
                        SaveSettings_Video();
                        presetIndex = i; // đồng bộ lại presetIndex
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            // --- Lặp lại video ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Lặp lại video:");
            ImGui::TableSetColumnIndex(1);

            ImGui::PushItemWidth(-1);
            if (RenderToggleCombo("##repeat_video_combo", v_Settings.repeatVideo)) {
                if (mpv) {
                    const char* value = v_Settings.repeatVideo ? "inf" : "no";
                    const char* cmd[] = { "set", "loop-file", value, nullptr };
                    mpv_command(mpv, cmd);
                }
                SaveSettings_Video();
            }
            ImGui::PopItemWidth();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Lặp lại list:");
            ImGui::TableSetColumnIndex(1);

            ImGui::PushItemWidth(-1);
            if (RenderToggleCombo("##repeat_list_combo", v_Settings.repeatlist)) {
                if (mpv) {
                    const char* value = v_Settings.repeatlist ? "force" : "no";
                    const char* cmd[] = { "set", "loop-playlist", value, nullptr };
                    mpv_command(mpv, cmd);
                }
                SaveSettings_Video();
            }
            ImGui::PopItemWidth();

            // --- Tự động phát tiếp ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Tự động phát tiếp:");
            ImGui::TableSetColumnIndex(1);

            ImGui::PushItemWidth(-1);
            if (RenderToggleCombo("##autoPlayNext_video_combo", v_Settings.autoPlayNext)) {
                if (mpv) {
                    const char* value = v_Settings.autoPlayNext ? "yes" : "no";
                    const char* cmd[] = { "set", "playlist-auto-advance", value, nullptr };
                    mpv_command(mpv, cmd);
                }
                SaveSettings_Video();
            }
            ImGui::PopItemWidth();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Theme Audio");
            ImGui::TableSetColumnIndex(1);
            
            ImGui::PushItemWidth(-1);
            if (RenderToggleCombo("##Theme Audio", audio_Theme))
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
}


void ApplyPlaybackSettings(mpv_handle * mpv) {
    if (!mpv) return;

    // 1. Thiết lập các thông số dạng số (Double)
    double volume = std::clamp(static_cast<double>(v_Settings.defaultVolume), 0.0, 100.0);
    mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &volume); 

    double speed = static_cast<double>(v_Settings.playbackSpeed);
    mpv_set_property(mpv, "speed", MPV_FORMAT_DOUBLE, &speed);

    double audiodelay = static_cast<double>(v_Settings.audiodelay);
    mpv_set_property(mpv, "audio-delay", MPV_FORMAT_DOUBLE, &audiodelay);

    // 2. Thiết lập Subtitles (Dùng mpv_set_property_string cho gọn và an toàn)
    const char* sub_vis = v_Settings.enableSubtitles ? "auto" : "no"; // mpv dùng "no" để tắt sub
    mpv_set_property_string(mpv, "sid", sub_vis);

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
    if (!file_local)
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


