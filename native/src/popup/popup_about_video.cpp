#include "reusable_popup.h"
#include"globals.h"
#include"utils.h"

#include "mpv/mpv_basic_formats.h"
#include "mpv/scripts/script_manager.h"
#include "mpv/mpv_custom_ui.h"

#include"popup_about_video.h"

#include"windows/windows_borderless_state.h"

#include <imgui.h>

// ------------ Các hàm hiển thị nội dung riêng -------------
void ShowMediaInfo() {

    static bool showFullUrl = false;

    ImGui::Text("Media Info");
    ImGui::Separator();

    if (ImGui::BeginTable("media_info", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {

        InfoRow("Title :", "%s", g_playbackStatus.Title.c_str());
        InfoRow("Media Title :", "%s", g_playbackStatus.mediaTitle.c_str());
        InfoRow("File :", "%s", g_playbackStatus.filename.c_str());
        InfoRow("Format :", "%s", g_playbackStatus.fileFormat.c_str());

        ImGui::EndTable();
    }


    // ====== STREAM URL (SPECIAL HANDLING) ======
    if (!g_playbackStatus.streamUrl.empty()) {

        std::string full = g_playbackStatus.streamUrl.c_str();
        std::string display = showFullUrl ? full : TextUtils::TruncateText(full, 80);

        ImGui::Spacing();
        ImGui::Text("Stream URL :");
        ImGui::Separator();

        if (ImGui::BeginChild("url_box", ImVec2(0, 80), true)) {

            ImGui::TextWrapped("%s", display.c_str());

            // Hover → hiện full
            if(SetDelayHover(ImGui::IsItemHovered(), 5.0, "Stream_URL_Hover")) {
                ImGui::SetTooltip("%s", full.c_str());
            }
        }
        ImGui::EndChild();

        // Buttons
        if (ImGui::Button(showFullUrl ? "Hide Full" : "Show Full")) {
            showFullUrl = !showFullUrl;
        }

        ImGui::SameLine();

        if (ImGui::Button("Copy")) {
            ImGui::SetClipboardText(full.c_str());
        }
    }
}

void ShowVideoInfo() {

    // ====== Overview ======
    ImGui::Text("Overview");
    ImGui::Separator();

    if (BeginInfoTable("video_overview")) {

    InfoRow("Light :", "%s", g_videoInfo.g_videoparams.vlight.c_str());
    InfoRow("Codec :", "%s", g_videoInfo.vcodec.c_str());
    InfoRow("Pixel Format :", "%s", g_videoInfo.g_videoparams.vpixfmt.c_str());
    InfoRow("Format :", "%s", g_videoInfo.video_format.c_str());
    InfoRow("HW Decode :", "%s", g_videoInfo.hwdec.c_str());

    EndInfoTable();
    }


    // ====== FPS / Performance ======
    ImGui::Spacing();
    ImGui::Text("Performance");
    ImGui::Separator();

    if (BeginInfoTable("video_perf")) {

    InfoRow("Current FPS :", "%.2f", g_videoInfo.currentFPS);
    InfoRow("Estimated FPS (mpv) :", "%.2f", g_videoInfo.estimated_vf_fps_mpv);
    InfoRow("Min / Max FPS :", "%.2f / %.2f" ,g_videoInfo.minFPS, g_videoInfo.maxFPS);
    EndInfoTable();
    }


    // ====== Resolution ======
    ImGui::Spacing();
    ImGui::Text("Resolution");
    ImGui::Separator();

    if (BeginInfoTable("video_resolution")) {

    InfoRow("Size :", "%dx%d", g_videoInfo.width, g_videoInfo.height);
    InfoRow("Display :", "%dx%d", g_videoInfo.g_videoparams.vdisp_w, g_videoInfo.g_videoparams.vdisp_h);
    InfoRow("Aspect Name :", "%s", g_videoInfo.g_videoparams.vaspect_name.c_str());
    InfoRow("SAR Name :", "%s", g_videoInfo.g_videoparams.vsar_name.c_str());
    InfoRow("Aspect Ratio :", "%.2f", g_videoInfo.g_videoparams.vaspect);

    EndInfoTable();
    }


    // ====== Crop ======
    ImGui::Spacing();
    ImGui::Text("Crop");
    ImGui::Separator();

    if (BeginInfoTable("video_crop")) {

    InfoRow("Crop X x Y :" , "%d x %d", g_videoInfo.g_videoparams.vcrop_x, g_videoInfo.g_videoparams.vcrop_y);
    InfoRow("Crop WH :", "%d x %d",g_videoInfo.g_videoparams.vcrop_w,g_videoInfo.g_videoparams.vcrop_h);
    EndInfoTable();
    }


    // ====== Color ======
    ImGui::Spacing();
    ImGui::Text("Color Info");
    ImGui::Separator();

    if (BeginInfoTable("video_color")) {

    InfoRow("Primaries", "%s", g_videoInfo.g_videoparams.vprimaries.c_str());
    InfoRow("Gamma", "%s", g_videoInfo.g_videoparams.vgamma.c_str());
    InfoRow("Matrix", "%s", g_videoInfo.g_videoparams.vcolormatrix.c_str());
    InfoRow("Levels", "%s", g_videoInfo.g_videoparams.vcolorlevels.c_str());

    EndInfoTable();
    }


    // ====== Advanced ======
    ImGui::Spacing();
    ImGui::Text("Advanced");
    ImGui::Separator();

    if (BeginInfoTable("video_advanced")) {

    InfoRow("Stereo In", "%s" ,g_videoInfo.g_videoparams.vstereo_in.c_str());
    InfoRow("Chroma Location", "%s", g_videoInfo.g_videoparams.vchroma_location.c_str());
    InfoRow("SAR / PAR", "%0.2f / %0.2f"," ", g_videoInfo.g_videoparams.vsar, g_videoInfo.g_videoparams.vpar);
    InfoRow("Signal Peak" , "%d", g_videoInfo.g_videoparams.vsig_peak);
    InfoRow("Avg BPP", "%d", g_videoInfo.g_videoparams.average_bpp);
    InfoRow("Bitrate", "%d kbps", g_videoInfo.vbitrate / 1000);

    EndInfoTable();
    }
}

void ShowAudioInfo() {

    // ====== Audio Devices ======
    if (!g_playbackStatus.g_audioDevices.empty()) {
        if (ImGui::CollapsingHeader("Audio Devices", ImGuiTreeNodeFlags_DefaultOpen)) {

            if (ImGui::BeginTable("audio_devices", 3,
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchSame)) {

                // Header
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Description");
                ImGui::TableSetupColumn("Active");
                
                ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor(2);

                for (auto& dev : g_playbackStatus.g_audioDevices) {

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", dev.name.c_str());

                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", dev.description.c_str());

                    ImGui::TableNextColumn();
                    bool active = (dev.name == g_videoInfo.audio_device);

                    ImVec4 col = active ? ImVec4(0.3f,1,0.3f,1) : ImVec4(0.6f,0.6f,0.6f,1);
                    ImGui::TextColored(col, active ? "Yes" : "No");
                }

                ImGui::EndTable();
            }
        }
    }

    // ====== Audio Info ======
    ImGui::Spacing();
    ImGui::Text("Audio Overview");
    ImGui::Separator();

    if (ImGui::BeginTable("audio_info", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {

        InfoRow("Client", "%s", g_playbackStatus.audio_client_name.c_str());
        InfoRow("Device", "%s", g_videoInfo.audio_device.c_str());
        InfoRow("Codec", "%s", g_videoInfo.acodec.c_str());
        InfoRow("Format", "%s", g_videoInfo.g_audioarams.aformat.c_str());
        ImGui::EndTable();
    }


    // ====== Audio Properties ======
    ImGui::Spacing();
    ImGui::Text("Properties");
    ImGui::Separator();

    if (ImGui::BeginTable("audio_props", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {

        InfoRow("Volume :", "%d%%", g_playbackStatus.volume);
        InfoRow("Delay Audio :", "%.2f s", g_videoInfo.audio_delay);
        InfoRow("Bitrate Audio :", "%d kbps", g_videoInfo.abitrate / 1000);
        InfoRow("Sample Rate :", "%d Hz", g_videoInfo.g_audioarams.asamplerate);
        InfoRow("Channels :", "%s (%d)",g_videoInfo.g_audioarams.achannels_str.c_str(),g_videoInfo.g_audioarams.channel_count);
        InfoRow("Channels HR :", "%s", g_videoInfo.g_audioarams.ahr_channels.c_str());

        ImGui::EndTable();
    }
}
void ShowTrackInfo() {
    ImGui::Text("Track List");
    ImGui::Separator();

    int index = 0;

    for (const auto& track : g_videoInfo.g_tracks) {
        ImGui::PushID(index++);

        // ====== CARD ======
        ImGui::BeginChild("track_card", ImVec2(0, 0), true);

        // ---- HEADER ----
        ImGui::Text("[%s #%d]", track.type.c_str(), track.id);
        ImGui::SameLine();
        ImGui::Text("%s", track.codec.c_str());

        if (!track.codec_profile.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", track.codec_profile.c_str());
        }

        // ---- STATUS BADGE ----
        if (track.selected) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "● Selected");
        }

        if (track.is_default) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f,0.7f,1,1), "● Default");
        }

        if (track.forced) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1,0.6f,0,1), "● Forced");
        }

        ImGui::Separator();

        // ====== QUICK INFO ======
        if (ImGui::BeginTable("track_summary", 2,
            ImGuiTableFlags_SizingStretchSame |
            ImGuiTableFlags_BordersInnerV)) {

            InfoRow("Language", "%s", track.language.c_str());
            InfoRow("Title", "%s", track.title.c_str());
            InfoRow("Format", "%s", track.format_name.c_str());
            InfoRow("External", "%s", track.external ? "Yes" : "No");
            InfoRow("Dependent", "%s", track.dependent ? "Yes" : "No");
            InfoRow("Album Art", "%s", track.albumart ? "Yes" : "No");
            InfoRow("Image Track", "%s", track.image ? "Yes" : "No");

            ImGui::EndTable();
        }

        // ====== DETAILS ======
        if (ImGui::TreeNode("Details")) {
            if (ImGui::BeginTable("track_details", 2,
                ImGuiTableFlags_SizingStretchSame |
                ImGuiTableFlags_BordersInnerV)) {

                InfoRow("Codec Description", "%s", track.codec_desc.c_str());
                InfoRow("Decoder", "%s", track.decoder.c_str());
                InfoRow("Decoder Description", "%s", track.decoder_desc.c_str());

                InfoRow("Visual Impaired", "%s", track.visual_impaired ? "Yes" : "No");
                InfoRow("Hearing Impaired", "%s", track.hearing_impaired ? "Yes" : "No");

                // ---- TYPE SPECIFIC ----
                if (track.type == "video") {
                    InfoRow("Resolution", "%dx%d", track.demux_w, track.demux_h);
                    InfoRow("FPS", "%.2f", track.demux_fps);
                } 
                else if (track.type == "audio") {
                    InfoRow("Sample Rate", "%d Hz", track.demux_samplerate);
                    InfoRow("Channels", "%s (%dch)", track.demux_channels.c_str(), track.demux_channel_count);
                }

                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        ImGui::EndChild();
        ImGui::Spacing();
        ImGui::PopID();
    }
}
void ShowPlaybackInfo() {

    // ====== STATE ======
    ImGui::Text("Playback State");
    ImGui::Separator();

    if (ImGui::BeginTable("playback_state", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {

        InfoRow("Status :","%s", PlaybackStateToString(GetPlaybackState()));
        InfoRow("Video Type :", "%s", VideoTypeToString(GetVideoType()));
        InfoRow("Loop Mode :", "%s", g_playbackStatus.loopMode.c_str());

        InfoRow("Has File :", "%s", g_playbackStatus.hasFile ? "Yes" : "No");
        InfoRow("Seekable :", "%s", g_playbackStatus.seekable ? "Yes" : "No");
        InfoRow("Idle :", "%s", g_playbackStatus.idle_active ? "Yes" : "No");

        InfoRow("Sub Visible :", "%s", g_playbackStatus.g_subinfo.sub_Visible ? "Yes" : "No");

        ImGui::EndTable();
    }


    // ====== TIMELINE ======
    ImGui::Spacing();
    ImGui::Text("Timeline");
    ImGui::Separator();

    float progress = 0.0f;
    if (g_playbackStatus.duration > 0.0f)
        progress = (float)(g_playbackStatus.timePos / g_playbackStatus.duration);

    // Progress bar
    ImGui::ProgressBar(progress, ImVec2(-1, 8));

    if (ImGui::BeginTable("playback_time", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {
        InfoRow("Current :", "%.2f / %.2f s", g_playbackStatus.timePos, g_playbackStatus.duration);
        InfoRow("Remaining :", "%.2f s", g_playbackStatus.time_remaining);
        InfoRow("Percent :", "%.2f%%", g_playbackStatus.percent_pos);

        ImGui::EndTable();
    }


    // ====== PLAYBACK PROPERTIES ======
    ImGui::Spacing();
    ImGui::Text("Properties");
    ImGui::Separator();

    if (ImGui::BeginTable("playback_props", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {

        InfoRow("Volume :", "%d%%", g_playbackStatus.volume);
        InfoRow("Speed :", "%.2fx", g_playbackStatus.speed);
        InfoRow("Subtitle Delay :", "%.2f s", g_playbackStatus.g_subinfo.sub_Delay);

        ImGui::EndTable();
    }
}

void ShowMetadata() {

    if (g_videoInfo.metadata.empty()) {
        ImGui::TextDisabled("No metadata available.");
    } else {
        for (auto& [key, value] : g_videoInfo.metadata) {
            ImGui::TextWrapped("%s: %s", key.c_str(), value.c_str());
        }
    }

}
void ShowNetworkInfo() {

    ImGui::Text("Network / Buffer");
    ImGui::Separator();

    if (ImGui::BeginTable("network_info", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {

        InfoRow("Cache Duration :", "%.2f s", g_playbackStatus.demuxer_cache_duration);
        InfoRow("Cache Time :", "%.2f s", g_playbackStatus.demuxer_cache_time);
        InfoRow("Audio Buffer :", "%.2f s", g_playbackStatus.audio_buffer);
        InfoRow("Bitrate :", "%.2f kbps", g_playbackStatus.demuxer_bitrate);
        InfoRow("Via Network :", "%s", g_playbackStatus.demuxer_via_network ? "Yes" : "No");

        ImGui::EndTable();
    }
    float bufferRatio = 0.0f;

    if (g_playbackStatus.demuxer_cache_duration > 0.0f) {
        bufferRatio = g_playbackStatus.demuxer_cache_time /
                    g_playbackStatus.demuxer_cache_duration;
    }

    // Clamp tránh lỗi
    bufferRatio = std::clamp(bufferRatio, 0.0f, 1.0f);

    ImGui::Spacing();
    ImGui::Text("Buffer");
    ImGui::ProgressBar(bufferRatio, ImVec2(-1, 8));

}
void ShowDuBugInFo(){
       
    ImGui::TextWrapped("=== Debug Info ===");
    // WinAPI
    RECT rcWin, rcClient;
    GetWindowRect(g_DragResizeState.hwnd_windown_main, &rcWin);
    GetClientRect(g_DragResizeState.hwnd_windown_main, &rcClient);
    POINT pt = { rcClient.left, rcClient.top };
    ClientToScreen(g_DragResizeState.hwnd_windown_main, &pt);
    OffsetRect(&rcClient, pt.x, pt.y);

    ImGui::Separator();
    ImGui::TextWrapped("WindowRect:  L:%d T:%d R:%d B:%d  (W:%d H:%d)",
                rcWin.left, rcWin.top, rcWin.right, rcWin.bottom,
                rcWin.right - rcWin.left, rcWin.bottom - rcWin.top);
    ImGui::TextWrapped("ClientRect:  L:%d T:%d R:%d B:%d  (W:%d H:%d)",
                rcClient.left, rcClient.top, rcClient.right, rcClient.bottom,
                rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);
    ImGui::TextWrapped("Border: L:%d T:%d R:%d B:%d",
                rcClient.left - rcWin.left,
                rcClient.top  - rcWin.top,
                rcWin.right   - rcClient.right,
                rcWin.bottom  - rcClient.bottom);


    // SDL window info
    Uint32 sdlFlags = 0;
    sdlFlags = SDL_GetWindowFlags(ctx.mainWindow);
    ImGui::TextWrapped("SDL Client: %dx%d", Windowlayout.WinW, Windowlayout.WinH);
    ImGui::TextWrapped("SDL Position: X:%d Y:%d", Windowlayout.WinX, Windowlayout.WinY);
    
    ImGui::TextWrapped("SDL Flags: 0x%08X", sdlFlags);
    
    ImGui::TextWrapped("Video Pos: %dx%d", (int)Windowlayout.VideoPos.x,(int)Windowlayout.VideoPos.y);
    ImGui::TextWrapped("Video Size: %dx%d", (int)Windowlayout.VideoSize.x, (int)Windowlayout.VideoSize.y);

    ImGui::Separator();
    ImGui::TextWrapped("IsMaximized: %s", g_DragResizeState.IsMax ? "Yes" : "No");
    ImGui::TextWrapped("IsFullscreen: %s", g_DragResizeState.IsFullscreen_video ? "Yes" : "No");
    ImGui::Separator();
    ImGui::TextWrapped("HitTest Zone: %s", g_DragResizeState.debugInfo.c_str());
    ImGui::Separator();

    // Lấy danh sách script dưới dạng struct (Giả sử bạn dùng GetAllScripts trả về vector hoặc map)
    auto allScripts = ScriptManager::Instance().GetAllScripts();

    ImGui::TextWrapped("Quản lý Scripts (%zu):", allScripts.size());

    float lineHeight = ImGui::GetFrameHeightWithSpacing(); 
    int visibleCount = 6; // Tăng lên một chút cho thoải mái
    float childHeight = lineHeight * visibleCount + ImGui::GetStyle().WindowPadding.y;

    if (ImGui::BeginChild("LoadedScriptsChild", ImVec2(0, childHeight), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        if (!allScripts.empty()) {
            // Nếu GetAllScripts trả về std::map<string, ScriptInfo>, dùng: for (auto& [path, info] : allScripts)
            // Ở đây giả định trả về std::vector<ScriptInfo> để code đơn giản:
            for (size_t i = 0; i < allScripts.size(); i++) {
                auto& script = allScripts[i];
                
                ImGui::PushID(script.path.c_str()); // Quan trọng: Tránh trùng ID giữa các dòng

    
                ImGui::Text("%zu. %s", i + 1, script.name.c_str());
                
                if (!script.enabled) ImGui::PopStyleColor();

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", script.path.c_str());
                }


                ImGui::PopID();
            }
        } else {
            ImGui::TextDisabled("(Không có script nào)");
        }
    }
    ImGui::EndChild();

    
}

void OpenVideoInfoPopup() {
    videoInfoPopup.Open("Video Info", [](bool& closePopup_VideoInFo) {
        ShowVideoInfoPopup(closePopup_VideoInFo);  // truyền ref
    });
}
void ShowVideoInfoPopup(bool& closePopup_VideoInFo) {

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,  ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,     ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

    ImGui::BeginChild("##PopupVideoInFo",avail,  true);

    if (ImGui::BeginTabBar("##InfoTabs",ImGuiTabBarFlags_FittingPolicyResizeDown)) {
        ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
        if (ImGui::BeginTabItem("Media"))   {
            BeginCard();
            ShowMediaInfo();
            EndCard();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Video"))   {
            BeginCard();
            ShowVideoInfo();
            EndCard();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Audio"))   {
            BeginCard();
            ShowAudioInfo();
            EndCard();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Playback")){
            BeginCard();
            ShowPlaybackInfo();
            EndCard();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Network")) {
            BeginCard();
            ShowNetworkInfo();
            EndCard();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Track"))   {
            BeginCard();
            ShowTrackInfo();
            EndCard();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Metadata"))   {
            BeginCard();
            ShowMetadata();
            EndCard();
            ImGui::EndTabItem();
        }

        if (g_DragResizeState.showDebug) {
            if (ImGui::BeginTabItem("DeBug")) 
            {
                BeginCard();
                ShowDuBugInFo();
                EndCard();
                ImGui::EndTabItem();
            }
        }
            
        ImGui::PopStyleColor();
        ImGui::EndTabBar();
    }

    ImGui::Separator();

    //if (ImGui::Button("Close")) {
    //    closePopup_VideoInFo = true;
    //}

    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
}
void RenderVideoInfoPopup(){
    // Render popup mỗi frame
    videoInfoPopup.Render();
}
