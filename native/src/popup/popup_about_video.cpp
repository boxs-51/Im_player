#include "reusable_popup.h"
#include"globals.h"
#include"utils.h"

#include "mpv/mpv_basic_formats.h"

#include "mpv/scripts/script_manager.h"

#include"popup_about_video.h"

#include"windows/windows_borderless_state.h"

#include <imgui.h>

void InfoRow(const char* label, const char* value) {
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);

    ImGui::TableNextColumn();
    ImGui::Text("%s", value ? value : "-");
}

void InfoRowFloat(const char* label, float value, const char* fmt = "%.2f") {
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);

    ImGui::TableNextColumn();
    ImGui::Text(fmt, value);
}

bool BeginInfoTable(const char* id) {
    return ImGui::BeginTable(id, 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV);
}

void EndInfoTable() {
    ImGui::EndTable();
}

void BeginCard() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1,1,1,1));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10,10));

    ImGui::BeginChild(ImGui::GetID("##card"), ImVec2(0, 0), true);
}

void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

std::string TruncateText(const std::string& text, size_t maxLen = 60) {
    if (text.length() <= maxLen) return text;
    return text.substr(0, maxLen) + "...";
}
// ------------ Các hàm hiển thị nội dung riêng -------------
void ShowMediaInfo() {

    static bool showFullUrl = false;

    ImGui::Text("Media Info");
    ImGui::Separator();

    if (ImGui::BeginTable("media_info", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {

        if (g_playbackStatus.Title)
            InfoRow("Title", g_playbackStatus.Title);

        if (g_playbackStatus.mediaTitle)
            InfoRow("Media Title", g_playbackStatus.mediaTitle);

        if (g_playbackStatus.filename)
            InfoRow("File", g_playbackStatus.filename);

        if (g_playbackStatus.fileFormat)
            InfoRow("Format", g_playbackStatus.fileFormat);

        ImGui::EndTable();
    }


    // ====== STREAM URL (SPECIAL HANDLING) ======
    if (g_playbackStatus.streamUrl) {

        std::string full = g_playbackStatus.streamUrl;
        std::string display = showFullUrl ? full : TruncateText(full, 80);

        ImGui::Spacing();
        ImGui::Text("Stream URL");
        ImGui::Separator();

        if (ImGui::BeginChild("url_box", ImVec2(0, 80), true)) {

            ImGui::TextWrapped("%s", display.c_str());

            // Hover → hiện full
            if (ImGui::IsItemHovered() && !showFullUrl) {
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

    InfoRow("Light", g_videoInfo.g_videoparams.vlight.c_str());
    InfoRow("Codec", g_videoInfo.vcodec);
    InfoRow("Pixel Format", g_videoInfo.g_videoparams.vpixfmt.c_str());
    InfoRow("Format", g_videoInfo.video_format);
    InfoRow("HW Decode", g_videoInfo.hwdec);

    EndInfoTable();
    }


    // ====== FPS / Performance ======
    ImGui::Spacing();
    ImGui::Text("Performance");
    ImGui::Separator();

    if (BeginInfoTable("video_perf")) {

    InfoRowFloat("Current FPS", g_videoInfo.currentFPS);
    InfoRowFloat("Estimated FPS (mpv)", g_videoInfo.estimated_vf_fps_mpv);

    ImGui::TableNextColumn(); ImGui::TextDisabled("Min / Max FPS");
    ImGui::TableNextColumn();
    ImGui::Text("%.2f / %.2f", g_videoInfo.minFPS, g_videoInfo.maxFPS);

    EndInfoTable();
    }


    // ====== Resolution ======
    ImGui::Spacing();
    ImGui::Text("Resolution");
    ImGui::Separator();

    if (BeginInfoTable("video_resolution")) {

    ImGui::TableNextColumn(); ImGui::TextDisabled("Size");
    ImGui::TableNextColumn();
    ImGui::Text("%dx%d",
        g_videoInfo.g_videoparams.vwidth,
        g_videoInfo.g_videoparams.vheight);

    ImGui::TableNextColumn(); ImGui::TextDisabled("Display");
    ImGui::TableNextColumn();
    ImGui::Text("%dx%d",
        g_videoInfo.g_videoparams.vdisp_w,
        g_videoInfo.g_videoparams.vdisp_h);

    InfoRow("Aspect Name", g_videoInfo.g_videoparams.vaspect_name.c_str());
    InfoRow("SAR Name", g_videoInfo.g_videoparams.vsar_name.c_str());

    InfoRowFloat("Aspect Ratio", g_videoInfo.g_videoparams.vaspect);

    EndInfoTable();
    }


    // ====== Crop ======
    ImGui::Spacing();
    ImGui::Text("Crop");
    ImGui::Separator();

    if (BeginInfoTable("video_crop")) {

    ImGui::TableNextColumn(); ImGui::TextDisabled("Crop XY");
    ImGui::TableNextColumn();
    ImGui::Text("%d x %d",
        g_videoInfo.g_videoparams.vcrop_x,
        g_videoInfo.g_videoparams.vcrop_y);

    ImGui::TableNextColumn(); ImGui::TextDisabled("Crop WH");
    ImGui::TableNextColumn();
    ImGui::Text("%d x %d",
        g_videoInfo.g_videoparams.vcrop_w,
        g_videoInfo.g_videoparams.vcrop_h);

    EndInfoTable();
    }


    // ====== Color ======
    ImGui::Spacing();
    ImGui::Text("Color Info");
    ImGui::Separator();

    if (BeginInfoTable("video_color")) {

    InfoRow("Primaries", g_videoInfo.g_videoparams.vprimaries.c_str());
    InfoRow("Gamma", g_videoInfo.g_videoparams.vgamma.c_str());
    InfoRow("Matrix", g_videoInfo.g_videoparams.vcolormatrix.c_str());
    InfoRow("Levels", g_videoInfo.g_videoparams.vcolorlevels.c_str());

    EndInfoTable();
    }


    // ====== Advanced ======
    ImGui::Spacing();
    ImGui::Text("Advanced");
    ImGui::Separator();

    if (BeginInfoTable("video_advanced")) {

    InfoRow("Stereo In", g_videoInfo.g_videoparams.vstereo_in.c_str());
    InfoRow("Chroma Location", g_videoInfo.g_videoparams.vchroma_location.c_str());

    ImGui::TableNextColumn(); ImGui::TextDisabled("SAR / PAR");
    ImGui::TableNextColumn();
    ImGui::Text("%d / %d",
        g_videoInfo.g_videoparams.vsar,
        g_videoInfo.g_videoparams.vpar);

    ImGui::TableNextColumn(); ImGui::TextDisabled("Signal Peak");
    ImGui::TableNextColumn();
    ImGui::Text("%d", g_videoInfo.g_videoparams.vsig_peak);

    ImGui::TableNextColumn(); ImGui::TextDisabled("Avg BPP");
    ImGui::TableNextColumn();
    ImGui::Text("%d", g_videoInfo.g_videoparams.average_bpp);

    ImGui::TableNextColumn(); ImGui::TextDisabled("Bitrate");
    ImGui::TableNextColumn();
    ImGui::Text("%d kbps", g_videoInfo.vbitrate / 1000);

    EndInfoTable();
    }
}

void ShowAudioInfo() {

    // ====== Audio Devices ======
    if (!g_audioDevices.empty()) {
        if (ImGui::CollapsingHeader("Audio Devices", ImGuiTreeNodeFlags_DefaultOpen)) {

            if (ImGui::BeginTable("audio_devices", 3,
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchSame)) {

                // Header
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Description");
                ImGui::TableSetupColumn("Active");
                ImGui::TableHeadersRow();

                for (auto& dev : g_audioDevices) {

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", dev.name.c_str());

                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", dev.description.c_str());

                    ImGui::TableNextColumn();
                    bool active = (dev.name == std::string(g_videoInfo.audio_device));

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

        InfoRow("Client", g_playbackStatus.audio_client_name);
        InfoRow("Device", g_videoInfo.audio_device);
        InfoRow("Codec", g_videoInfo.acodec);
        InfoRow("Format", g_videoInfo.g_audioarams.aformat.c_str());
        InfoRow("Output", g_videoInfo.a_out);
        InfoRow("Filter", g_videoInfo.a_filter);

        ImGui::EndTable();
    }


    // ====== Audio Properties ======
    ImGui::Spacing();
    ImGui::Text("Properties");
    ImGui::Separator();

    if (ImGui::BeginTable("audio_props", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {

        InfoRowFloat("Delay (s)", g_videoInfo.audio_delay);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextDisabled("Bitrate");
        ImGui::TableNextColumn();
        ImGui::Text("%d kbps", g_videoInfo.abitrate / 1000);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextDisabled("Sample Rate");
        ImGui::TableNextColumn();
        ImGui::Text("%d Hz", g_videoInfo.g_audioarams.asamplerate);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextDisabled("Channels");
        ImGui::TableNextColumn();
        ImGui::Text("%s (%d)",
            g_videoInfo.g_audioarams.achannels_str.c_str(),
            g_videoInfo.g_audioarams.channel_count);

        InfoRow("Channels HR", g_videoInfo.g_audioarams.ahr_channels.c_str());

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

        ImGui::Separator();

        // ====== QUICK INFO ======
        if (ImGui::BeginTable("track_summary", 2,
            ImGuiTableFlags_SizingStretchSame |
            ImGuiTableFlags_BordersInnerV)) {

            InfoRow("Language", track.language.c_str());
            InfoRow("Title", track.title.c_str());
            InfoRow("Format", track.format_name.c_str());

            ImGui::EndTable();
        }

        // ====== DETAILS ======
        if (ImGui::TreeNode("Details")) {

            if (ImGui::BeginTable("track_details", 2,
                ImGuiTableFlags_SizingStretchSame |
                ImGuiTableFlags_BordersInnerV)) {

                InfoRow("Codec Desc", track.codec_desc.c_str());
                InfoRow("Decoder", track.decoder.c_str());
                InfoRow("Decoder Desc", track.decoder_desc.c_str());

                InfoRow("External", track.external ? "Yes" : "No");
                InfoRow("Dependent", track.dependent ? "Yes" : "No");
                InfoRow("Forced", track.forced ? "Yes" : "No");

                // ---- TYPE SPECIFIC ----
                if (track.type == "video") {

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextDisabled("Resolution");
                    ImGui::TableNextColumn();
                    ImGui::Text("%dx%d", track.demux_w, track.demux_h);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextDisabled("FPS");
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", track.demux_fps);
                }
                else if (track.type == "audio") {

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextDisabled("Sample Rate");
                    ImGui::TableNextColumn();
                    ImGui::Text("%d Hz", track.demux_samplerate);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextDisabled("Channels");
                    ImGui::TableNextColumn();
                    ImGui::Text("%s (%dch)",
                        track.demux_channels.c_str(),
                        track.demux_channel_count);
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

        InfoRow("Status", PlaybackStateToString(GetPlaybackState()));
        InfoRow("Video Type", VideoTypeToString(GetVideoType()));
        InfoRow("Loop Mode", g_playbackStatus.loopMode);

        InfoRow("Has File", g_playbackStatus.hasFile ? "Yes" : "No");
        InfoRow("Seekable", g_playbackStatus.seekable ? "Yes" : "No");
        InfoRow("Idle", g_playbackStatus.idle_active ? "Yes" : "No");

        InfoRow("Sub Visible", g_playbackStatus.g_subinfo.sub_Visible ? "Yes" : "No");

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

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextDisabled("Current");
        ImGui::TableNextColumn();
        ImGui::Text("%.2f / %.2f s",
            g_playbackStatus.timePos,
            g_playbackStatus.duration);

        InfoRowFloat("Remaining", g_playbackStatus.time_remaining);
        InfoRowFloat("Percent", g_playbackStatus.percent_pos);

        ImGui::EndTable();
    }


    // ====== PLAYBACK PROPERTIES ======
    ImGui::Spacing();
    ImGui::Text("Properties");
    ImGui::Separator();

    if (ImGui::BeginTable("playback_props", 2,
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_BordersInnerV)) {

        InfoRowFloat("Speed", g_playbackStatus.speed, "%.2fx");
        InfoRowFloat("Subtitle Delay", g_playbackStatus.g_subinfo.sub_Delay);

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

        InfoRowFloat("Cache Duration (s)", g_playbackStatus.demuxer_cache_duration);
        InfoRowFloat("Cache Time (s)", g_playbackStatus.demuxer_cache_time);
        InfoRowFloat("Audio Buffer (s)", g_playbackStatus.audio_buffer);
        InfoRowFloat("Bitrate", g_playbackStatus.demuxer_bitrate);

        InfoRow("Via Network", g_playbackStatus.demuxer_via_network ? "Yes" : "No");

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

        if (g_DragResizeState.showDebug) {if (ImGui::BeginTabItem("DeBug")) {ShowDuBugInFo();ImGui::EndTabItem();}}
            
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
