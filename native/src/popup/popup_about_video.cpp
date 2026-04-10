#include "reusable_popup.h"
#include"globals.h"
#include"utils.h"

#include "mpv/mpv_basic_formats.h"
#include "mpv/mpv_custom_ui.h"
#include "mpv/mpv_ui.h"

#include "mpv/scripts/script_manager.h"
#include"popup_about_video.h"

#include"windows/windows_borderless_state.h"

#include <imgui.h>

// ------------ Các hàm hiển thị nội dung riêng -------------
void ShowMediaInfo() {

    static bool showFullUrl = false;

    ImGui::Text("Media Info");
    ImGui::Separator();

    if (CusTomImGui::BeginInfoTable("media_info")) {

        CusTomImGui::InfoRow("Title :", "%s", g_playbackStatus.Title.c_str());
        CusTomImGui::InfoRow("Media Title :", "%s", g_playbackStatus.mediaTitle.c_str());
        CusTomImGui::InfoRow("File :", "%s", g_playbackStatus.filename.c_str());
        CusTomImGui::InfoRow("Format :", "%s", g_playbackStatus.fileFormat.c_str());
        CusTomImGui::InfoRow("Working Directory :", "%s", g_playbackStatus.working_directory.c_str());
        

        CusTomImGui::EndInfoTable();
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
            ShowTooltipDelayed(full.c_str(), ImGui::IsItemHovered(), 5.0, "Stream_URL_Hover");
        }
        ImGui::EndChild();
        ImGui::Spacing();

        // Buttons
        if (CusTomImGui::ModernButton(showFullUrl ? "Hide Full" : "Show Full", ImVec2(90, 0))) {
            showFullUrl = !showFullUrl;
        }

        ImGui::SameLine();

        if (CusTomImGui::ModernButton("Copy", ImVec2(80, 0))) {
            ImGui::SetClipboardText(full.c_str());
        }
    }
}

void ShowVideoInfo() {

    // ====== Overview ======
    ImGui::Text("Overview");
    ImGui::Separator();

    if (CusTomImGui::BeginInfoTable("video_overview")) {

    CusTomImGui::InfoRow("Light :", "%s", g_videoInfo.g_videoparams.vlight.c_str());
    CusTomImGui::InfoRow("Codec :", "%s", g_videoInfo.vcodec.c_str());
    CusTomImGui::InfoRow("Pixel Format :", "%s", g_videoInfo.g_videoparams.vpixfmt.c_str());
    CusTomImGui::InfoRow("Format :", "%s", g_videoInfo.video_format.c_str());
    CusTomImGui::InfoRow("HW Decode :", "%s", g_videoInfo.hwdec.c_str());
    CusTomImGui::InfoRow("Video OutPut :", "%s", g_videoInfo.v_out.c_str());

    CusTomImGui::EndInfoTable();
    }


    // ====== FPS / Performance ======
    ImGui::Spacing();
    ImGui::Text("Performance");
    ImGui::Separator();

    if (CusTomImGui::BeginInfoTable("video_perf")) {

    CusTomImGui::InfoRow("Current FPS :", "%.2f", g_videoInfo.currentFPS);
    CusTomImGui::InfoRow("Estimated FPS (mpv) :", "%.2f", g_videoInfo.estimated_vf_fps_mpv);
    CusTomImGui::EndInfoTable();
    }


    // ====== Resolution ======
    ImGui::Spacing();
    ImGui::Text("Resolution");
    ImGui::Separator();

    if (CusTomImGui::BeginInfoTable("video_resolution")) {

    CusTomImGui::InfoRow("Size :", "%dx%d", g_videoInfo.width, g_videoInfo.height);
    CusTomImGui::InfoRow("Display :", "%dx%d", g_videoInfo.g_videoparams.vdisp_w, g_videoInfo.g_videoparams.vdisp_h);
    CusTomImGui::InfoRow("Aspect Name :", "%s", g_videoInfo.g_videoparams.vaspect_name.c_str());
    CusTomImGui::InfoRow("SAR Name :", "%s", g_videoInfo.g_videoparams.vsar_name.c_str());
    CusTomImGui::InfoRow("Aspect Ratio :", "%.2f", g_videoInfo.g_videoparams.vaspect);

    CusTomImGui::EndInfoTable();
    }


    // ====== Crop ======
    ImGui::Spacing();
    ImGui::Text("Crop");
    ImGui::Separator();

    if (CusTomImGui::BeginInfoTable("video_crop")) {

    CusTomImGui::InfoRow("Crop X x Y :" , "%d x %d", g_videoInfo.g_videoparams.vcrop_x, g_videoInfo.g_videoparams.vcrop_y);
    CusTomImGui::InfoRow("Crop WH :", "%d x %d",g_videoInfo.g_videoparams.vcrop_w,g_videoInfo.g_videoparams.vcrop_h);
    CusTomImGui::EndInfoTable();
    }


    // ====== Color ======
    ImGui::Spacing();
    ImGui::Text("Color Info");
    ImGui::Separator();

    if (CusTomImGui::BeginInfoTable("video_color")) {

    CusTomImGui::InfoRow("Primaries", "%s", g_videoInfo.g_videoparams.vprimaries.c_str());
    CusTomImGui::InfoRow("Gamma", "%s", g_videoInfo.g_videoparams.vgamma.c_str());
    CusTomImGui::InfoRow("Matrix", "%s", g_videoInfo.g_videoparams.vcolormatrix.c_str());
    CusTomImGui::InfoRow("Levels", "%s", g_videoInfo.g_videoparams.vcolorlevels.c_str());

    CusTomImGui::EndInfoTable();
    }


    // ====== Advanced ======
    ImGui::Spacing();
    ImGui::Text("Advanced");
    ImGui::Separator();

    if (CusTomImGui::BeginInfoTable("video_advanced")) {

        CusTomImGui::InfoRow("Stereo In", "%s" ,g_videoInfo.g_videoparams.vstereo_in.c_str());
        CusTomImGui::InfoRow("Chroma Location", "%s", g_videoInfo.g_videoparams.vchroma_location.c_str());
        CusTomImGui::InfoRow("SAR / PAR", "%0.2f / %0.2f"," ", g_videoInfo.g_videoparams.vsar, g_videoInfo.g_videoparams.vpar);
        CusTomImGui::InfoRow("Signal Peak" , "%d", g_videoInfo.g_videoparams.vsig_peak);
        CusTomImGui::InfoRow("Avg BPP", "%d", g_videoInfo.g_videoparams.average_bpp);
        CusTomImGui::InfoRow("Bitrate", "%d kbps", g_videoInfo.vbitrate / 1000);

        CusTomImGui::EndInfoTable();
    }
}

void ShowAudioInfo() {

    // ====== Audio Devices ======
    if (!g_playbackStatus.g_audioDevices.empty()) {

        if (CusTomImGui::ModernCollapsingHeader("Audio Devices",ImGuiTreeNodeFlags_DefaultOpen)) {
            // 1. Định nghĩa cấu trúc bảng
            std::vector <CusTomImGui::TableCol> cols = {
                {"Device Name", 180.0f},
                {"Description", 0.0f},   // Stretch
                {"Status", 80.0f}
            };

            // 2. Bắt đầu bảng
            if (CusTomImGui::BeginListTable("audio_devices_v2", cols)) {
                
                for (auto& dev : g_playbackStatus.g_audioDevices) {
                    bool active = (dev.name == g_videoInfo.audio_device);

                    // 3. Bắt đầu hàng
                    CusTomImGui::BeginListRow();

                    // Cột 1 (Đã tự động chuyển Column ở BeginListRow)
                    if (active) ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "● %s", dev.name.c_str());
                    else ImGui::Text("%s", dev.name.c_str());

                    // Cột 2
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", dev.description.empty() ? "N/A" : dev.description.c_str());

                    // Cột 3
                    ImGui::TableNextColumn();
                    if (active) ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Active");
                    else ImGui::TextDisabled("Idle");

                    // 4. Xử lý click cho toàn bộ hàng
                    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    //if (IsRowClicked()) {
                    //    g_videoInfo.audio_device = dev.name;
                        // Thực hiện lệnh đổi thiết bị tại đây
                    //}

                    CusTomImGui::EndListRow();
                }
                CusTomImGui::EndListTable();
            }
        }
    }

    // ====== Audio Info ======
    ImGui::Spacing();
    ImGui::Text("Audio Overview");
    ImGui::Separator();
    if(CusTomImGui::BeginInfoTable("audio_info")){

        CusTomImGui::InfoRow("Audio Client", "%s", g_playbackStatus.audio_client_name.c_str());
        CusTomImGui::InfoRow("Audio Device", "%s", g_videoInfo.audio_device.c_str());
        CusTomImGui::InfoRow("Audio Codec", "%s", g_videoInfo.acodec.c_str());
        CusTomImGui::InfoRow("Audio Format", "%s", g_videoInfo.g_audioarams.aformat.c_str());
        CusTomImGui::InfoRow("Audio OutPut", "%s", g_videoInfo.a_out.c_str());
        CusTomImGui::InfoRow("Audio Fillter", "%s", g_videoInfo.a_filter.c_str());


        CusTomImGui::EndInfoTable();
    }


    // ====== Audio Properties ======
    ImGui::Spacing();
    ImGui::Text("Audio Properties");
    ImGui::Separator();

    if(CusTomImGui::BeginInfoTable("audio_props")){

        CusTomImGui::InfoRow("Mute :" , "%s", g_playbackStatus.isMuted ? "Yes" : "No") ;   
        CusTomImGui::InfoRow("Volume :", "%d%%", g_playbackStatus.volume);
        CusTomImGui::InfoRow("Delay Audio :", "%.2f s", g_videoInfo.audio_delay);
        CusTomImGui::InfoRow("Bitrate Audio :", "%d kbps", g_videoInfo.abitrate / 1000);
        CusTomImGui::InfoRow("Sample Rate :", "%d Hz", g_videoInfo.g_audioarams.asamplerate);
        CusTomImGui::InfoRow("Channels :", "%s (%d)",g_videoInfo.g_audioarams.achannels_str.c_str(),g_videoInfo.g_audioarams.channel_count);
        CusTomImGui::InfoRow("Channels HR :", "%s", g_videoInfo.g_audioarams.ahr_channels.c_str());

        CusTomImGui::EndInfoTable();
    }
}
void ShowTrackInfo() {
    ImGui::Text("Track List");
    ImGui::Separator();

    for (const auto& track : g_videoInfo.g_tracks) {
        // Sử dụng ID của track để PushID cho an toàn
        std::string unique_id = track.common.type + "_" + std::to_string(track.common.id);
        ImGui::PushID(unique_id.c_str());

        // ====== CARD ======
        // Sử dụng Child window với chiều cao cố định hoặc tự động
        ImGui::BeginChild("track_card", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);

        // ---- HEADER (Phần chung) ----
        ImGui::Text("[%s #%d]", track.common.type.c_str(), track.common.id);
        ImGui::SameLine();
        ImGui::Text("%s", track.common.codec.c_str());

        if (!track.common.codec_profile.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", track.common.codec_profile.c_str());
        }

        // ---- STATUS BADGES ----
        if (track.common.selected) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), " ● Selected");
        }
        if (track.common.is_default) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), " ● Default");
        }
        if (track.common.forced) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), " ● Forced");
        }

        ImGui::Separator();

        // ====== QUICK INFO (Thông tin tóm tắt) ======
        if (ImGui::BeginTable("track_summary", 2, ImGuiTableFlags_SizingStretchSame)) {
            
            CusTomImGui::InfoRow("Language", "%s", track.common.language.empty() ? "unknown" : track.common.language.c_str());
            CusTomImGui::InfoRow("Title", "%s", track.common.title.empty() ? "N/A" : track.common.title.c_str());
            
            // Hiển thị thông số kỹ thuật nhanh dựa trên loại track
            if (track.common.type == "video") {
                CusTomImGui::InfoRow("Resolution", "%dx%d", track.video.demux_w, track.video.demux_h);
                if (track.video.albumart) CusTomImGui::InfoRow("Type", "Album Art");
            } 
            else if (track.common.type == "audio") {
                CusTomImGui::InfoRow("Channels", "%s (%d ch)", track.audio.demux_channels.c_str(), track.audio.demux_channel_count);
            }

            ImGui::EndTable();
        }

        // ====== DETAILS (Phần mở rộng) ======

        if (CusTomImGui::ModernTreeNode(("Full Details##" + std::to_string(track.common.id)).c_str())){
            if(CusTomImGui::BeginInfoTable("track_details_full")){
                
                // Common Details
                CusTomImGui::InfoRow("Codec Desc", "%s", track.common.codec_desc.c_str());
                CusTomImGui::InfoRow("Decoder", "%s", track.common.decoder.c_str());
                CusTomImGui::InfoRow("FF-Index", "%d", track.common.ff_index);
                CusTomImGui::InfoRow("External", "%s", track.common.external ? "Yes" : "No");

                // Video Details
                if (track.common.type == "video" || track.common.type == "image") {
                    CusTomImGui::InfoRow("FPS", "%.3f", track.video.demux_fps);
                    CusTomImGui::InfoRow("Format", "%s", track.video.format_name.c_str());
                    CusTomImGui::InfoRow("Is Image", "%s", track.video.image ? "Yes" : "No");
                }
                
                // Audio Details
                if (track.common.type == "audio") {
                    CusTomImGui::InfoRow("Sample Rate", "%d Hz", track.audio.demux_samplerate);
                    CusTomImGui::InfoRow("Format", "%s", track.audio.format_name.c_str());
                }

                // Accessibility
                if (track.access.hearing_impaired || track.access.visual_impaired) {
                    CusTomImGui::InfoRow("Accessibility", "%s%s", 
                        track.access.hearing_impaired ? "[Hearing] " : "",
                        track.access.visual_impaired ? "[Visual]" : "");
                }

                CusTomImGui::EndInfoTable();
            }
            CusTomImGui::EndModernTreeNode();
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
    if(CusTomImGui::BeginInfoTable("playback_state")){

        CusTomImGui::InfoRow("Status :","%s", PlaybackStateToString(GetPlaybackState()));
        CusTomImGui::InfoRow("Video Type :", "%s", VideoTypeToString(GetVideoType()));
        CusTomImGui::InfoRow("Loop Mode :", "%s", g_playbackStatus.loopMode.c_str());

        CusTomImGui::InfoRow("Has File :", "%s", g_playbackStatus.hasFile ? "Yes" : "No");
        CusTomImGui::InfoRow("Seekable :", "%s", g_playbackStatus.seekable ? "Yes" : "No");
        CusTomImGui::InfoRow("Idle :", "%s", g_playbackStatus.idle_active ? "Yes" : "No");

        CusTomImGui::InfoRow("Sub Visible :", "%s", g_playbackStatus.g_subinfo.sub_Visible ? "Yes" : "No");

        CusTomImGui::EndInfoTable();
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

    if(CusTomImGui::BeginInfoTable("playback_time")){

        CusTomImGui::InfoRow("Current :", "%.2f / %.2f s", g_playbackStatus.timePos, g_playbackStatus.duration);
        CusTomImGui::InfoRow("Stream Pos :", "%d Bytes", g_playbackStatus.stream_pos);
        CusTomImGui::InfoRow("Remaining :", "%.2f s", g_playbackStatus.time_remaining);
        CusTomImGui::InfoRow("Percent :", "%.2f%%", g_playbackStatus.percent_pos);

        CusTomImGui::EndInfoTable();
    }


    // ====== PLAYBACK PROPERTIES ======
    ImGui::Spacing();
    ImGui::Text("Properties");
    ImGui::Separator();

    if(CusTomImGui::BeginInfoTable("playback_props")){

        CusTomImGui::InfoRow("Volume :", "%d%%", g_playbackStatus.volume);
        CusTomImGui::InfoRow("Speed :", "%.2fx", g_playbackStatus.speed);
        CusTomImGui::InfoRow("Subtitle Delay :", "%.2f s", g_playbackStatus.g_subinfo.sub_Delay);

        CusTomImGui::EndInfoTable();
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

    if(CusTomImGui::BeginInfoTable("network_info")){

        
        CusTomImGui::InfoRow("Cache Buffer State:", "%d s", g_playbackStatus.cache_buffering_state);
        CusTomImGui::InfoRow("Cache Duration :", "%.2f s", g_playbackStatus.demuxer_cache_duration);
        CusTomImGui::InfoRow("Cache Time :", "%.2f s", g_playbackStatus.demuxer_cache_time);
        CusTomImGui::InfoRow("Audio Buffer :", "%.2f s", g_playbackStatus.audio_buffer);
        CusTomImGui::InfoRow("Audio Buffer1 :", "%.2f s", g_playbackStatus.audio_demuxer);
        CusTomImGui::InfoRow("Bitrate :", "%.2f kbps", g_playbackStatus.demuxer_bitrate);
        CusTomImGui::InfoRow("Via Network :", "%s", g_playbackStatus.demuxer_via_network ? "Yes" : "No");

        CusTomImGui::EndInfoTable();
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
    //ImVec2 avail = ImGui::GetContentRegionAvail();

    // Sử dụng Helper cho Child (Vùng bao ngoài)
    //if (BeginModernChild("##PopupVideoInFo", avail, true)) {
        
        // Sử dụng Helper cho TabBar
        if (CusTomImGui::BeginModernTabBar("##InfoTabs")) {
            
            // Một mảng cấu trúc để lặp qua các Tab (Giúp code gọn hơn nữa)
            struct Tab { const char* Name; void (*Func)(); };
            Tab tabs[] = {
                {"Media", ShowMediaInfo},
                {"Video", ShowVideoInfo},
                {"Audio", ShowAudioInfo},
                {"Playback", ShowPlaybackInfo},
                {"Network", ShowNetworkInfo},
                {"Track", ShowTrackInfo},
                {"Metadata", ShowMetadata}
            };

            for (auto& tab : tabs) {
                if (ImGui::BeginTabItem(tab.Name)) {
                    ImGui::Dummy(ImVec2(0, 10)); // Thêm khoảng trống trên đầu mỗi card
                    if(CusTomImGui::BeginCard()){
                        tab.Func();
                        CusTomImGui::EndCard();
                    }
                    ImGui::EndTabItem();
                }
            }

            // Tab Debug đặc biệt
            if (g_DragResizeState.showDebug && ImGui::BeginTabItem("Debug")) {
                if(CusTomImGui::BeginCard()){
                    ShowDuBugInFo();
                    CusTomImGui::EndCard();
                }
                ImGui::EndTabItem();
            }

            CusTomImGui::EndModernTabBar();
        }
        
    //    EndModernChild();
    //}
}
void RenderVideoInfoPopup(){
    // Render popup mỗi frame
    videoInfoPopup.Render();
}
