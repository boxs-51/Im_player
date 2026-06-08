#include "reusable_popup.h"
#include "globals.h"
#include "utils.h"

#include <mpv/mpv_render_video.h>
#include "mpv/mpv_basic_formats.h"
#include <gui/gui.h>
#include "mpv/mpv_ui.h"
#include <mpv/mpv_data.h>

#include "mpv/scripts/script_manager.h"
#include"popup_about_video.h"

#include"windows/windows_borderless_state.h"

#include <imgui.h>
#include <functional>
static MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
static VideoInfo& g_videoInfo = GetVideoInfo();
static DragResizeState& g_DragResizeState = GetDragResizeState();
// ------------ Các hàm hiển thị nội dung riêng -------------
void ShowMediaInfo() {

    static bool showFullUrl = false;

    CSImGui::ModernHeader("Media Info");

    if (CSImGui::BeginInfoTable("media_info")) {

        CSImGui::InfoRow("Title :", "%s", g_playbackStatus.Title.c_str());
        CSImGui::InfoRow("Media Title :", "%s", g_playbackStatus.mediaTitle.c_str());
        CSImGui::InfoRow("File :", "%s", g_playbackStatus.filename.c_str());
        CSImGui::InfoRow("Format :", "%s", g_playbackStatus.fileFormat.c_str());
        CSImGui::InfoRow("Working Directory :", "%s", g_playbackStatus.working_directory.c_str());
        

        CSImGui::EndInfoTable();
    }


    // ====== STREAM URL (SPECIAL HANDLING) ======
    if (!g_playbackStatus.streamUrl.empty()) {

        std::string full = g_playbackStatus.streamUrl.c_str();
        std::string display = showFullUrl ? full : TextUtils::TruncateText(full, 80);

        CSImGui::ModernHeader("Stream URL :");

        if (ImGui::BeginChild("url_box", ImVec2(0, 80), true)) {

            ImGui::TextWrapped("%s", display.c_str());

            // Hover → hiện full
            CSImGui::ShowTooltipDelayed(full.c_str(), ImGui::IsItemHovered(), 5.0);

        }
        ImGui::EndChild();
        ImGui::Spacing();

        // Buttons
        if (CSImGui::ModernButton(showFullUrl ? "Hide Full" : "Show Full", ImVec2(90, 0))) {
            showFullUrl = !showFullUrl;
        }

        ImGui::SameLine();

        if (CSImGui::ModernButton("Copy", ImVec2(80, 0))) {
            ImGui::SetClipboardText(full.c_str());
        }
    }
}

void ShowVideoInfo() {

    // ====== Overview ======
    CSImGui::ModernHeader("Overview");

    if (CSImGui::BeginInfoTable("video_overview")) {

    CSImGui::InfoRow("Light :", "%s", g_videoInfo.g_videoparams.vlight.c_str());
    CSImGui::InfoRow("Codec :", "%s", g_videoInfo.vcodec.c_str());
    CSImGui::InfoRow("Pixel Format :", "%s", g_videoInfo.g_videoparams.vpixfmt.c_str());
    CSImGui::InfoRow("Format :", "%s", g_videoInfo.video_format.c_str());
    CSImGui::InfoRow("HW Decode :", "%s", g_videoInfo.hwdec.c_str());
    CSImGui::InfoRow("Video OutPut :", "%s", g_videoInfo.v_out.c_str());

    CSImGui::EndInfoTable();
    }


    // ====== FPS / Performance ======
    CSImGui::ModernHeader("Performance");

    if (CSImGui::BeginInfoTable("video_perf")) {

    CSImGui::InfoRow("Current FPS :", "%.2f", g_videoInfo.currentFPS);
    #ifdef RENDER_MPV_THREAD
    CSImGui::InfoRow("Frame Render FPS :", "%.2f", renderThread.framerender.load());
    #endif
    CSImGui::InfoRow("Estimated FPS (mpv) :", "%.2f", g_videoInfo.estimated_vf_fps_mpv);
    CSImGui::EndInfoTable();
    }


    // ====== Resolution ======
    CSImGui::ModernHeader("Resolution");

    if (CSImGui::BeginInfoTable("video_resolution")) {

    CSImGui::InfoRow("Size :", "%dx%d", g_videoInfo.width, g_videoInfo.height);
    CSImGui::InfoRow("Display :", "%dx%d", g_videoInfo.g_videoparams.vdisp_w, g_videoInfo.g_videoparams.vdisp_h);
    CSImGui::InfoRow("Aspect Name :", "%s", g_videoInfo.g_videoparams.vaspect_name.c_str());
    CSImGui::InfoRow("SAR Name :", "%s", g_videoInfo.g_videoparams.vsar_name.c_str());
    CSImGui::InfoRow("Aspect Ratio :", "%.2f", g_videoInfo.g_videoparams.vaspect);

    CSImGui::EndInfoTable();
    }


    // ====== Crop ======
    CSImGui::ModernHeader("Crop");

    if (CSImGui::BeginInfoTable("video_crop")) {

    CSImGui::InfoRow("Crop X x Y :" , "%d x %d", g_videoInfo.g_videoparams.vcrop_x, g_videoInfo.g_videoparams.vcrop_y);
    CSImGui::InfoRow("Crop WH :", "%d x %d",g_videoInfo.g_videoparams.vcrop_w,g_videoInfo.g_videoparams.vcrop_h);
    CSImGui::EndInfoTable();
    }


    // ====== Color ======
    CSImGui::ModernHeader("Color Info");

    if (CSImGui::BeginInfoTable("video_color")) {

    CSImGui::InfoRow("Primaries", "%s", g_videoInfo.g_videoparams.vprimaries.c_str());
    CSImGui::InfoRow("Gamma", "%s", g_videoInfo.g_videoparams.vgamma.c_str());
    CSImGui::InfoRow("Matrix", "%s", g_videoInfo.g_videoparams.vcolormatrix.c_str());
    CSImGui::InfoRow("Levels", "%s", g_videoInfo.g_videoparams.vcolorlevels.c_str());

    CSImGui::EndInfoTable();
    }


    // ====== Advanced ======
    CSImGui::ModernHeader("Advanced");

    if (CSImGui::BeginInfoTable("video_advanced")) {

        CSImGui::InfoRow("Stereo In", "%s" ,g_videoInfo.g_videoparams.vstereo_in.c_str());
        CSImGui::InfoRow("Chroma Location", "%s", g_videoInfo.g_videoparams.vchroma_location.c_str());
        CSImGui::InfoRow("SAR / PAR", "%0.2f / %0.2f"," ", g_videoInfo.g_videoparams.vsar, g_videoInfo.g_videoparams.vpar);
        CSImGui::InfoRow("Signal Peak" , "%d", g_videoInfo.g_videoparams.vsig_peak);
        CSImGui::InfoRow("Avg BPP", "%d", g_videoInfo.g_videoparams.average_bpp);
        CSImGui::InfoRow("Bitrate", "%d kbps", g_videoInfo.vbitrate / 1000);

        CSImGui::EndInfoTable();
    }
}

void ShowAudioInfo() {

    // ====== Audio Devices ======
    if (!g_playbackStatus.g_audioDevices.empty()) {

        if (CSImGui::ModernCollapsingHeader("Audio Devices",ImGuiTreeNodeFlags_DefaultOpen)) {
            // 1. Định nghĩa cấu trúc bảng
            std::vector <CSImGui::TableCol> cols = {
                {"Device Name", 180.0f},
                {"Description", 0.0f},   // Stretch
                {"Status", 80.0f}
            };

            // 2. Bắt đầu bảng
            if (CSImGui::BeginListTable("audio_devices_v2", cols)) {
                
                for (auto& dev : g_playbackStatus.g_audioDevices) {
                    bool active = (dev.name == g_videoInfo.audio_device);

                    // 3. Bắt đầu hàng
                    CSImGui::BeginListRow();

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

                    CSImGui::EndListRow();
                }
                CSImGui::EndListTable();
            }
        }
    }

    // ====== Audio Info ======
    CSImGui::ModernHeader("Audio Overview");

    if(CSImGui::BeginInfoTable("audio_info")){

        CSImGui::InfoRow("Audio Client", "%s", g_playbackStatus.audio_client_name.c_str());
        CSImGui::InfoRow("Audio Device", "%s", g_videoInfo.audio_device.c_str());
        CSImGui::InfoRow("Audio Codec", "%s", g_videoInfo.acodec.c_str());
        CSImGui::InfoRow("Audio Format", "%s", g_videoInfo.g_audioarams.aformat.c_str());
        CSImGui::InfoRow("Audio OutPut", "%s", g_videoInfo.a_out.c_str());
        CSImGui::InfoRow("Audio Fillter", "%s", g_videoInfo.a_filter.c_str());


        CSImGui::EndInfoTable();
    }


    // ====== Audio Properties ======
    CSImGui::ModernHeader("Audio Properties");

    if(CSImGui::BeginInfoTable("audio_props")){

        CSImGui::InfoRow("Mute :" , "%s", g_playbackStatus.isMuted ? "Yes" : "No") ;   
        CSImGui::InfoRow("Volume :", "%d%%", g_playbackStatus.volume);
        CSImGui::InfoRow("Delay Audio :", "%.2f s", g_videoInfo.audio_delay);
        CSImGui::InfoRow("Bitrate Audio :", "%d kbps", g_videoInfo.abitrate / 1000);
        CSImGui::InfoRow("Sample Rate :", "%d Hz", g_videoInfo.g_audioarams.asamplerate);
        CSImGui::InfoRow("Channels :", "%s (%d)",g_videoInfo.g_audioarams.achannels_str.c_str(),g_videoInfo.g_audioarams.channel_count);
        CSImGui::InfoRow("Channels HR :", "%s", g_videoInfo.g_audioarams.ahr_channels.c_str());

        CSImGui::EndInfoTable();
    }
}
void ShowTrackInfo() {
    CSImGui::ModernHeader("Track List");

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
            
            CSImGui::InfoRow("Language", "%s", track.common.language.empty() ? "unknown" : track.common.language.c_str());
            CSImGui::InfoRow("Title", "%s", track.common.title.empty() ? "N/A" : track.common.title.c_str());
            
            // Hiển thị thông số kỹ thuật nhanh dựa trên loại track
            if (track.common.type == "video") {
                CSImGui::InfoRow("Resolution", "%dx%d", track.video.demux_w, track.video.demux_h);
                if (track.video.albumart) CSImGui::InfoRow("Type", "Album Art");
            } 
            else if (track.common.type == "audio") {
                CSImGui::InfoRow("Channels", "%s (%d ch)", track.audio.demux_channels.c_str(), track.audio.demux_channel_count);
            }

            ImGui::EndTable();
        }

        // ====== DETAILS (Phần mở rộng) ======

        if (CSImGui::ModernTreeNode(("Full Details##" + std::to_string(track.common.id)).c_str())){
            if(CSImGui::BeginInfoTable("track_details_full")){
                
                // Common Details
                CSImGui::InfoRow("Codec Desc", "%s", track.common.codec_desc.c_str());
                CSImGui::InfoRow("Decoder", "%s", track.common.decoder.c_str());
                CSImGui::InfoRow("FF-Index", "%d", track.common.ff_index);
                CSImGui::InfoRow("External", "%s", track.common.external ? "Yes" : "No");

                // Video Details
                if (track.common.type == "video" || track.common.type == "image") {
                    CSImGui::InfoRow("FPS", "%.3f", track.video.demux_fps);
                    CSImGui::InfoRow("Format", "%s", track.video.format_name.c_str());
                    CSImGui::InfoRow("Is Image", "%s", track.video.image ? "Yes" : "No");
                }
                
                // Audio Details
                if (track.common.type == "audio") {
                    CSImGui::InfoRow("Sample Rate", "%d Hz", track.audio.demux_samplerate);
                    CSImGui::InfoRow("Format", "%s", track.audio.format_name.c_str());
                }

                // Accessibility
                if (track.access.hearing_impaired || track.access.visual_impaired) {
                    CSImGui::InfoRow("Accessibility", "%s%s", 
                        track.access.hearing_impaired ? "[Hearing] " : "",
                        track.access.visual_impaired ? "[Visual]" : "");
                }

                CSImGui::EndInfoTable();
            }
            CSImGui::EndModernTreeNode();
        }

        ImGui::EndChild();
        ImGui::Spacing();
        ImGui::PopID();
    }
}
void ShowPlaybackInfo() {

    // ====== STATE ======
    CSImGui::ModernHeader("Playback State");

    if(CSImGui::BeginInfoTable("playback_state")){

        CSImGui::InfoRow("Status :","%s", PlaybackStateToString(GetPlaybackState()));
        CSImGui::InfoRow("Video Type :", "%s", VideoTypeToString(GetVideoType()));
        CSImGui::InfoRow("Loop Mode :", "%s", g_playbackStatus.loopMode.c_str());

        CSImGui::InfoRow("Has File :", "%s", g_playbackStatus.hasFile ? "Yes" : "No");
        CSImGui::InfoRow("Seekable :", "%s", g_playbackStatus.seekable ? "Yes" : "No");
        CSImGui::InfoRow("Idle :", "%s", g_playbackStatus.idle_active ? "Yes" : "No");

        CSImGui::InfoRow("Sub Visible :", "%s", g_playbackStatus.g_subinfo.sub_Visible ? "Yes" : "No");

        CSImGui::EndInfoTable();
    }


    // ====== TIMELINE ======
    CSImGui::ModernHeader("Timeline");

    float progress = 0.0f;
    if (g_playbackStatus.duration > 0.0f)
        progress = (float)(g_playbackStatus.timePos / g_playbackStatus.duration);

    // Progress bar
    ImGui::ProgressBar(progress, ImVec2(-1, 8));

    if(CSImGui::BeginInfoTable("playback_time")){

        CSImGui::InfoRow("Current :", "%.2f / %.2f s", g_playbackStatus.timePos, g_playbackStatus.duration);
        CSImGui::InfoRow("Stream Pos :", "%d Bytes", g_playbackStatus.stream_pos);
        CSImGui::InfoRow("Remaining :", "%.2f s", g_playbackStatus.time_remaining);
        CSImGui::InfoRow("Percent :", "%.2f%%", g_playbackStatus.percent_pos);

        CSImGui::EndInfoTable();
    }


    // ====== PLAYBACK PROPERTIES ======
    CSImGui::ModernHeader("Properties");

    if(CSImGui::BeginInfoTable("playback_props")){

        CSImGui::InfoRow("Volume :", "%d%%", g_playbackStatus.volume);
        CSImGui::InfoRow("Speed :", "%.2fx", g_playbackStatus.speed);
        CSImGui::InfoRow("Subtitle Delay :", "%.2f s", g_playbackStatus.g_subinfo.sub_Delay);

        CSImGui::EndInfoTable();
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

    CSImGui::ModernHeader("Network / Buffer");

    if(CSImGui::BeginInfoTable("network_info")){

        
        CSImGui::InfoRow("Cache Buffer State:", "%d s", g_playbackStatus.cache_buffering_state);
        CSImGui::InfoRow("Cache Duration :", "%.2f s", g_playbackStatus.demuxer_cache_duration);
        CSImGui::InfoRow("Cache Time :", "%.2f s", g_playbackStatus.demuxer_cache_time);
        CSImGui::InfoRow("Audio Buffer :", "%.2f s", g_playbackStatus.audio_buffer);
        CSImGui::InfoRow("Audio Buffer1 :", "%.2f s", g_playbackStatus.audio_demuxer);
        CSImGui::InfoRow("Bitrate :", "%.2f kbps", g_playbackStatus.demuxer_bitrate);
        CSImGui::InfoRow("Via Network :", "%s", g_playbackStatus.demuxer_via_network ? "Yes" : "No");

        CSImGui::EndInfoTable();
    }
    float bufferRatio = 0.0f;

    if (g_playbackStatus.demuxer_cache_duration > 0.0f) {
        bufferRatio = g_playbackStatus.demuxer_cache_time /
                    g_playbackStatus.demuxer_cache_duration;
    }

    // Clamp tránh lỗi
    bufferRatio = std::clamp(bufferRatio, 0.0f, 1.0f);

    CSImGui::ModernHeader("Buffer");
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

void OpenVideoInfoPopup(ReusablePopup& popup) {
    popup.Open("Video Info", [](bool& closePopup_VideoInFo) {
        ShowVideoInfoPopup(closePopup_VideoInFo);  // truyền ref
    });
}
void ShowVideoInfoPopup(bool& closePopup_VideoInFo) {
    //ImVec2 avail = ImGui::GetContentRegionAvail();

    // Sử dụng Helper cho Child (Vùng bao ngoài)
    //if (BeginModernChild("##PopupVideoInFo", avail, true)) {

        // Sử dụng Helper cho TabBar
        if (CSImGui::BeginModernTabBar("##InfoTabs")) {
            
            // Một mảng cấu trúc để lặp qua các Tab (Giúp code gọn hơn nữa)
            struct Tab { 
                const char* Name; 
                std::function<void()> Func; 
            };
            Tab tabs[] = {
                {"Media",    [&]() { ShowMediaInfo(); }},
                {"Video",    [&]() { ShowVideoInfo(); }},
                {"Audio",    [&]() { ShowAudioInfo(); }},
                {"Playback", [&]() { ShowPlaybackInfo(); }},
                {"Network",  [&]() { ShowNetworkInfo(); }},
                {"Track",    [&]() { ShowTrackInfo(); }},
                {"Metadata", [&]() { ShowMetadata(); }}
            };

            for (auto& tab : tabs) {
                if (CSImGui::ModernTabItem(tab.Name)) {
                    ImGui::Dummy(ImVec2(0, 10)); // Thêm khoảng trống trên đầu mỗi card
                    if(CSImGui::BeginCard()){
                        tab.Func();
                        CSImGui::EndCard();
                    }
                    CSImGui::EndModernTabItem();
                }
            }

            // Tab Debug đặc biệt
            if (g_DragResizeState.showDebug && CSImGui::ModernTabItem("Debug")) {
                if(CSImGui::BeginCard()){
                    ShowDuBugInFo();
                    CSImGui::EndCard();
                }
                CSImGui::EndModernTabItem();
            }

            CSImGui::EndModernTabBar();
        }
        
    //    EndModernChild();
    //}
}
void RenderVideoInfoPopup(ReusablePopup& popup){
    // Render popup mỗi frame
    popup.Render();
}
