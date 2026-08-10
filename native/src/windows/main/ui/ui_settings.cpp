#include "ui_settings.h"
#include "ui_widgets.h"
#include "player/mpv_data.h"
#include "player/session/PlayerSession.h"
#include "gui/gui.h"
#include "WindowRuntime.h"
#include "settings_manager.h"
#include "globals.h"
#include "utils.h"
#include <cmath>


static VideoAudioFormats& all_formats = GetVideoAudioFormats();
enum class SettingsPage { Main, ResolutionQuality, AudioQuality, PlaybackSpeed, Options };
enum class OptionsPage { Main, Subtitles };
static SettingsPage current_page = SettingsPage::Main;
static OptionsPage current_options_page = OptionsPage::Main;

static float page_anim = 1.0f; 
static SettingsPage last_page = SettingsPage::Main;

static auto ChangePage = [](SettingsPage next) {
    if (current_page != next) {
        last_page = current_page;
        current_page = next;
        page_anim = 0.0f;
    }
};

void ResolutionQualityPage(WindowRuntime* runtime, VideoAudioFormats &formats, VideoType videotype, float scale) {
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0,0,0,0));
    auto& Cfg = ConfigManager::Instance();
    MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
    if (ImGui::BeginChild("##res_scroll_area", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove)) {
        for (int i = 0; i < (int)all_formats.video.full_labels.size(); ++i) {
            bool is_active = (all_formats.video_index == i);
            const char* label = all_formats.video.full_labels[i].c_str();

            UI_SelectableItem(label, is_active, scale, [&]() {
                if (all_formats.video_index != i) {
                    all_formats.video_index = i;
                    all_formats.active_video = all_formats.video.formats[i];
                    std::string selectedFormat = all_formats.video.formats[i];
                    Cfg.UpdateVideoSettings([selectedFormat](AppSettings& s) {
                        s.selectedFormat = selectedFormat;
                    });
                    if (g_playbackStatus.hasFile) {
                        if(videotype == VideoType::Live) {
                            if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
                                runtime->resource.playersession->GetCommander()->SetPropertyString("ytdl-format", all_formats.video.ids[i]);
                        } else {
                            pendingSeekTime = g_playbackStatus.timePos;
                            std::string selectedResolutio = Cfg.GetVideoSettings().selectedFormat + "+" + Cfg.GetVideoSettings().selectedAudio;
                            Cfg.UpdateVideoSettings([selectedResolutio](AppSettings& s) {
                                s.selectedResolution = selectedResolutio;
                            });
                            if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
                                runtime->resource.playersession->GetCommander()->SetPropertyString("ytdl-format", selectedResolutio);
                        }
                        std::string cmd = "playlist-play-index " + std::to_string(g_playbackStatus.g_PlayingIndex);
                        if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) runtime->resource.playersession->GetCommander()->Exec(cmd);
                    }
                    Cfg.SaveVideo();
                }
            });

            if (is_active && ImGui::IsWindowAppearing()) {
                ImGui::SetScrollHereY();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void AudioQualityPage(WindowRuntime* runtime, VideoAudioFormats &formats, VideoType videotype, float scale) {
    auto& Cfg = ConfigManager::Instance();
    MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
    if (ImGui::BeginChild("##audio_scroll_area", ImVec2(0, 0), false)) {
        for (int i = 0; i < (int)all_formats.audio.full_labels.size(); ++i) {
            bool is_active = (all_formats.audio_index == i);
            const char* label = all_formats.audio.full_labels[i].c_str();
            
            UI_SelectableItem(label, is_active, scale, [&]() {
                if (all_formats.audio_index != i) {
                    all_formats.audio_index = i;
                    all_formats.active_audio = all_formats.audio.formats[i];
                    std::string selectedAudio = all_formats.audio.formats[i];
                    Cfg.UpdateVideoSettings([selectedAudio](AppSettings& s) {
                        s.selectedAudio = selectedAudio;
                    });
                    if (g_playbackStatus.hasFile) {
                        if(videotype == VideoType::Live){
                            if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
                                runtime->resource.playersession->GetCommander()->SetPropertyString("ytdl-format", all_formats.audio.ids[i]);
                        }else{
                            pendingSeekTime = g_playbackStatus.timePos;
                            std::string selectedResolution = Cfg.GetVideoSettings().selectedFormat + "+" + Cfg.GetVideoSettings().selectedAudio;
                            Cfg.UpdateVideoSettings([selectedResolution](AppSettings& s) {
                                s.selectedAudio = selectedResolution;
                            });
                            if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
                                runtime->resource.playersession->GetCommander()->SetPropertyString("ytdl-format", selectedResolution);
                        }
                        
                        std::string cmd = "playlist-play-index " + std::to_string(g_playbackStatus.g_PlayingIndex);
                        if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) runtime->resource.playersession->GetCommander()->Exec(cmd);
                    }
                    Cfg.SaveVideo();
                }
            });

            if (is_active && ImGui::IsWindowAppearing()) {
                ImGui::SetScrollHereY();
            }
        }
    }
    ImGui::EndChild();
}

void PlaybackSpeedPage(WindowRuntime* runtime, float scale) {
    auto& Cfg = ConfigManager::Instance();
    MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
    float current_speed = (float)g_playbackStatus.speed;
    ImGui::Indent(10 * scale);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Tốc độ tùy chỉnh: %.2fx", current_speed);
    ImGui::Unindent(10 * scale);
    ImGui::Dummy(ImVec2(0, 5 * scale));

    UI_SliderSpeed("SpeedSlider", &current_speed, 0.25f, 4.0f, scale, [&](float new_speed) {
        Cfg.UpdateVideoSettings([new_speed](AppSettings& s){
            s.playbackSpeed = new_speed;
        });
        if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
            runtime->resource.playersession->GetCommander()->SetSpeed((double)new_speed);
        Cfg.SaveVideo();
    });

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10 * scale));
    
    ImGui::Indent(10 * scale);
    ImGui::TextDisabled("CHỌN NHANH");
    ImGui::Unindent(10 * scale);
    ImGui::Dummy(ImVec2(0, 5 * scale));

    static const float speeds[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f };
    
    if (ImGui::BeginChild("##speed_list", ImVec2(0, 0), false)) {
        for (float s : speeds) {
            char buf[16]; 
            snprintf(buf, sizeof(buf), "%.2fx", s);
            
            bool is_active = (fabs(current_speed - s) < 0.01f);
            
            UI_SelectableItem(buf, is_active, scale, [&]() {
                Cfg.UpdateVideoSettings([s](AppSettings& ss) {
                    ss.playbackSpeed = s;
                });
                if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
                    runtime->resource.playersession->GetCommander()->SetSpeed((double)s);
                Cfg.SaveVideo();
            });

            if (is_active && ImGui::IsWindowAppearing()) {
                ImGui::SetScrollHereY();
            }
        }
    }
    ImGui::EndChild();
}

//void OptionsPage() {}

void RenderIOCHSidebar(WindowRuntime* runtime, ImVec2 videoPos, ImVec2 videoSize, bool open, ImVec2 iconPos) {
    static float anim = 0.0f;
    UpdateHoverAnim(anim, open, 15.0f);
    
    if (anim < 0.01f) {
        current_page = SettingsPage::Main; 
        return;
    }
    VideoInfo& g_videoInfo = GetVideoInfo();
    auto& Cfg = ConfigManager::Instance();
    auto videoCfg = Cfg.GetVideoSettings();

    float scaleFactor = videoSize.y / 720.0f;
    scaleFactor = std::max(scaleFactor, 1.0f);
    scaleFactor = std::min(scaleFactor, 2.0f);

    float targetWidth = 260.0f * scaleFactor;
    float targetHeight = 350.0f * scaleFactor;

    if (current_page == SettingsPage::ResolutionQuality || 
        current_page == SettingsPage::AudioQuality ) {
        targetWidth = 300.0f * scaleFactor;
        targetHeight = 370.0f * scaleFactor;
    }
    ImVec2 size_target = ImVec2(targetWidth, targetHeight);
    static ImVec2 current_window_size = ImVec2(targetWidth, targetHeight);
    current_window_size = ImLerp(current_window_size, size_target, SMOOTH_LERP(15.0f, ImGui::GetIO().DeltaTime));

    ImVec2 windowSize(current_window_size);

    float padding = 30.0f * scaleFactor;
    float maxAllowedW = videoSize.x - (padding * 2.0f);
    float maxAllowedH = videoSize.y - (padding * 2.0f);

    if (windowSize.x > maxAllowedW) windowSize.x = maxAllowedW;
    if (windowSize.y > maxAllowedH) windowSize.y = maxAllowedH;

    current_window_size.x = ImMin(current_window_size.x, maxAllowedW);
    current_window_size.y = ImMin(current_window_size.y, maxAllowedH);

    ImVec2 windowPos(
        videoPos.x + videoSize.x - windowSize.x - padding,
        iconPos.y - windowSize.y - padding
    );

    if (windowPos.y < videoPos.y) {
        windowPos.y = iconPos.y + 40.0f * scaleFactor; 
    }

    float minX = videoPos.x + padding;
    float maxX = videoPos.x + videoSize.x - windowSize.x - padding;
    windowPos.x = ImClamp(windowPos.x, minX, maxX);

    float minY = videoPos.y + padding;
    float maxY = videoPos.y + videoSize.y - windowSize.y - padding;
    windowPos.y = ImClamp(windowPos.y, minY, maxY);

    ImRect iconRect(iconPos, iconPos + ImVec2(40 * scaleFactor, 40 * scaleFactor));
    ImRect windowRect(windowPos, windowPos + windowSize);

    if (windowRect.Overlaps(iconRect)) {
        windowPos.x = iconPos.x - windowSize.x - padding;
        windowPos.x = ImClamp(windowPos.x, videoPos.x + padding, maxX);
    }

    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(windowSize);
    ImGui::SetNextWindowBgAlpha(0.92f * anim);

    CSImGui::PushModernWindowStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim);

    if (ImGui::Begin("##SettingsSidebar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        page_anim = ImMin(page_anim + ImGui::GetIO().DeltaTime * 6.0f, 1.0f);

        float slide_up = (1.0f - page_anim) * 20.0f * scaleFactor;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + slide_up);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim * page_anim);

        VideoType videotype = GetVideoType();
        switch (current_page) {
            case SettingsPage::Main:
            {
                UI_GroupHeader("Chất lượng", scaleFactor);
                if (videotype != VideoType::Local) {
                    const char* res_label = all_formats.video.short_labels.empty() ? "N/A" : all_formats.video.short_labels[all_formats.video_index].c_str();
                    UI_MenuItem("Độ phân giải", res_label, scaleFactor, [&]() { ChangePage(SettingsPage::ResolutionQuality); });
                }
                if (videotype != VideoType::Local) {
                    const char* res_label = all_formats.audio.short_labels.empty() ? "N/A" : all_formats.audio.short_labels[all_formats.audio_index].c_str();
                    UI_MenuItem("Chất lượng âm thanh", res_label, scaleFactor, [&]() { ChangePage(SettingsPage::AudioQuality); });
                }
    
                char speed_buf[16];
                snprintf(speed_buf, sizeof(speed_buf), "%.2fx", videoCfg.playbackSpeed);
                UI_MenuItem("Tốc độ phát", speed_buf, scaleFactor, [&]() { ChangePage(SettingsPage::PlaybackSpeed); });

                UI_MenuItem("Tùy chọn nâng cao", "Thiết lập", scaleFactor, [&]() { ChangePage(SettingsPage::Options); });

                ImGui::Spacing();
                UI_GroupHeader("Tùy chọn", scaleFactor);
                bool enableSubtitles = videoCfg.enableSubtitles;
                UI_Toggle("Phụ đề", &enableSubtitles, scaleFactor, g_videoInfo.hasSubtitles, [&](bool s) {
                    Cfg.UpdateVideoSettings([enableSubtitles](AppSettings& s) {
                        s.enableSubtitles = enableSubtitles;
                    });
                    if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
                        runtime->resource.playersession->GetCommander()->SetPropertyString("sub-visibility", s ? "yes" : "no");
                    Cfg.SaveVideo();
                });
                bool repeatVideo = videoCfg.repeatVideo;
                UI_Toggle("Lặp lại video", &repeatVideo, scaleFactor, true, [&](bool s) {
                    Cfg.UpdateVideoSettings([repeatVideo](AppSettings& s) {
                        s.repeatVideo = repeatVideo;
                    });
                    if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
                        runtime->resource.playersession->GetCommander()->SetPropertyString("loop-file", s ? "inf" : "no");
                    Cfg.SaveVideo();
                });
                bool autoPlayNext = videoCfg.autoPlayNext;
                UI_Toggle("Tự động phát tiếp", &autoPlayNext, scaleFactor, true, [&](bool s) {
                    Cfg.UpdateVideoSettings([autoPlayNext](AppSettings& s) {
                        s.autoPlayNext = autoPlayNext;
                    });
                    if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
                        runtime->resource.playersession->GetCommander()->SetPropertyString("playlist-auto-advance", s ? "yes" : "no");
                    Cfg.SaveVideo();
                });

                if (auto* renderer = runtime->resource.playersession->GetRenderer()) {
                    bool isAudioVis = renderer->IsAudioVisualizerEnabled();
                    UI_Toggle("Trình chiếu âm thanh ", &isAudioVis, scaleFactor, true, [renderer](bool enabled) {
                        renderer->SetAudioVisualizerEnabled(enabled);
                    });
                }
                break;
            }
        
            case SettingsPage::ResolutionQuality:
            {
                if (CSImGui::ModernSelectable("< Quay lại", false, 0, ImVec2(0, 25))) ChangePage(SettingsPage::Main);
                ImGui::Separator();
                ImGui::Spacing();

                ResolutionQualityPage(runtime, all_formats, videotype, scaleFactor);
                break;
            }

            case SettingsPage::AudioQuality:
            {
                if (CSImGui::ModernSelectable("< Quay lại", false, 0, ImVec2(0, 25))) ChangePage(SettingsPage::Main);
                ImGui::Separator();
                ImGui::Spacing();

                AudioQualityPage(runtime, all_formats, videotype, scaleFactor);
                break;
            }
        
            case SettingsPage::PlaybackSpeed:
            {
                if (CSImGui::ModernSelectable("< Quay lại", false, 0, ImVec2(0, 25))) ChangePage(SettingsPage::Main);
                ImGui::Separator();
                ImGui::Spacing();

                PlaybackSpeedPage(runtime, scaleFactor);
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

                        bool enableSubtitles = videoCfg.enableSubtitles;
                        UI_Toggle("Phụ đề", &enableSubtitles, scaleFactor, g_videoInfo.hasSubtitles, [&](bool s) {
                            Cfg.UpdateVideoSettings([enableSubtitles](AppSettings& s) {
                                s.enableSubtitles = enableSubtitles;
                            });
                            if (runtime && runtime->resource.playersession && runtime->resource.playersession->GetCommander()) 
                                runtime->resource.playersession->GetCommander()->SetPropertyString("sub-visibility", s ? "yes" : "no");
                            Cfg.SaveVideo();
                        });

                        break;
                    }
                }
                break;
            }
        }

        ImGui::PopStyleVar();
        ImGui::End();
    }
    CSImGui::PopModernWindowStyle();
    ImGui::PopStyleVar();
}

void ApplyPlaybackSettings(WindowRuntime* runtime) {
    if (!runtime || !runtime->resource.playersession || !runtime->resource.playersession->GetCommander()) return;
    auto* commander = runtime->resource.playersession->GetCommander();

    auto& Cfg = ConfigManager::Instance();
    auto videoCfg = Cfg.GetVideoSettings();

    commander->SetVolume(videoCfg.defaultVolume);

    double speed = (double)videoCfg.playbackSpeed;
    commander->SetSpeed(speed);

    double audiodelay = (double)videoCfg.audiodelay;
    commander->SetAudioDelay(audiodelay);

    const char* sub_vis = videoCfg.enableSubtitles ? "yes" : "no";
    commander->SetPropertyString("sub-visibility", sub_vis);

    const char* repeat_mode = videoCfg.repeatVideo ? "inf" : "no";
    commander->SetPropertyString("loop-file", repeat_mode);

    const char* auto_next_mode = videoCfg.autoPlayNext ? "yes" : "no";
    commander->SetPropertyString("playlist-auto-advance", auto_next_mode);

    const char* loop_list = videoCfg.repeatlist ? "force" : "no";
    commander->SetPropertyString("loop-playlist", loop_list);
}