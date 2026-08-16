#include "reusable_popup.h"
#include "globals.h"
#include "utils.h"

#include "gui/gui.h"

#include "player/scripts/script_manager.h"
#include "player/session/PlayerManager.h"
#include "player/session/PlayerSession.h"
#include "player/PlayerUtils.h"

#include "popup_about_video.h"
#include "windows/WindowRuntime.h"

#include "WindowManager.h"


#include <imgui.h>
#include <functional>

// ------------ Các hàm hiển thị nội dung riêng -------------
void ShowMediaInfo(WindowRuntime* runtime) {

    static bool showFullUrl = false;

    auto* player_session = runtime->resource.GetPlayerSession();
    if (!player_session) return;
    auto* state = player_session->GetState();
    if (!state) return;

    auto media = state->GetMediaModel();

    CSImGui::ModernHeader("Media Info");

    if (CSImGui::BeginInfoTable("media_info")) {
        CSImGui::InfoRow("Title :", "%s", media.title.c_str());
        CSImGui::InfoRow("Media Title :", "%s", media.mediaTitle.c_str());
        CSImGui::InfoRow("File :", "%s", media.filename.c_str());
        CSImGui::InfoRow("Format :", "%s", media.fileFormat.c_str());
        CSImGui::InfoRow("Working Directory :", "%s", media.working_directory.c_str());
        CSImGui::EndInfoTable();
    }


    // ====== STREAM URL (SPECIAL HANDLING) ======
    if (!media.streamUrl.empty()) {

        std::string full = media.streamUrl.c_str();
        std::string display = showFullUrl ? full : TextUtils::TruncateText(full, 80);

        CSImGui::ModernHeader("Stream URL :");

        if (ImGui::BeginChild("url_box", ImVec2(0, 80), true)) {

            ImGui::TextWrapped("%s", display.c_str());

            // Hover → hiện full
            //CSImGui::ShowTooltipDelayed(full.c_str(), ImGui::IsItemHovered(), 5.0);
            CSImGui::ToolTip(full.c_str(), 5.0 , ToolTipFlags_Animation);

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

void ShowVideoInfo(WindowRuntime* runtime) {
    auto* player_session = runtime->resource.GetPlayerSession();
    if(!player_session) return;
    auto* state = player_session->GetState();
    if (!state) return;
    auto video = state->GetVideoModel();
    // ====== Overview ======
    CSImGui::ModernHeader("Overview");

    if (CSImGui::BeginInfoTable("video_overview")) {

        CSImGui::InfoRow("Light :", "%s", video.params.vlight.c_str());
        CSImGui::InfoRow("Codec :", "%s", video.codec.vcodec.c_str());
        CSImGui::InfoRow("Pixel Format :", "%s", video.params.vpixfmt.c_str());
        CSImGui::InfoRow("Format :", "%s", video.codec.video_format.c_str());
        CSImGui::InfoRow("HW Decode :", "%s", video.codec.hwdec.c_str());
        CSImGui::InfoRow("Video OutPut :", "%s", video.codec.v_out.c_str());

        CSImGui::EndInfoTable();
    }


    // ====== FPS / Performance ======
    CSImGui::ModernHeader("Performance");

    if (CSImGui::BeginInfoTable("video_perf")) {

    CSImGui::InfoRow("Main Loop Rate :", "%d", main_loop_rate);

    if (runtime && runtime->windowloop){
        CSImGui::InfoRow("WinDow ID :", "%d", (int)runtime->info.id);
        CSImGui::InfoRow("WinDow Loop Rate :", "%d", (int)runtime->windowloop->getFPS());
    }
    
    #ifdef RENDER_MPV_THREAD
    if (auto* player_render_thread = player_session->GetRenderThread())
    {
        CSImGui::InfoRow("Frame Render FPS :", "%.2f", player_render_thread->state.framerender.load());
    }
    #endif
    CSImGui::InfoRow("Estimated FPS (mpv) :", "%.2f", video.stats.estimated_vf_fps_mpv);
    CSImGui::EndInfoTable();
    }


    // ====== Resolution ======
    CSImGui::ModernHeader("Resolution");

    if (CSImGui::BeginInfoTable("video_resolution")) {

    CSImGui::InfoRow("Size :", "%dx%d", video.dimensions.width, video.dimensions.height);
    CSImGui::InfoRow("Display :", "%dx%d", video.params.vdisp_w, video.params.vdisp_h);
    CSImGui::InfoRow("Aspect Name :", "%s", video.params.vaspect_name.c_str());
    CSImGui::InfoRow("SAR Name :", "%s", video.params.vsar_name.c_str());
    CSImGui::InfoRow("Aspect Ratio :", "%.2f", video.dimensions.aspect);

    CSImGui::EndInfoTable();
    }


    // ====== Crop ======
    CSImGui::ModernHeader("Crop");

    if (CSImGui::BeginInfoTable("video_crop")) {

    CSImGui::InfoRow("Crop X x Y :" , "%d x %d", video.params.vcrop_x, video.params.vcrop_y);
    CSImGui::InfoRow("Crop WH :", "%d x %d",video.params.vcrop_w, video.params.vcrop_h);
    CSImGui::EndInfoTable();
    }


    // ====== Color ======
    CSImGui::ModernHeader("Color Info");

    if (CSImGui::BeginInfoTable("video_color")) {

    CSImGui::InfoRow("Primaries", "%s", video.params.vprimaries.c_str());
    CSImGui::InfoRow("Gamma", "%s", video.params.vgamma.c_str());
    CSImGui::InfoRow("Matrix", "%s", video.params.vcolormatrix.c_str());
    CSImGui::InfoRow("Levels", "%s", video.params.vcolorlevels.c_str());

    CSImGui::EndInfoTable();
    }


    // ====== Advanced ======
    CSImGui::ModernHeader("Advanced");

    if (CSImGui::BeginInfoTable("video_advanced")) {

        CSImGui::InfoRow("Stereo In", "%s" ,video.params.vstereo_in.c_str());
        CSImGui::InfoRow("Chroma Location", "%s", video.params.vchroma_location.c_str());
        CSImGui::InfoRow("SAR / PAR", "%0.2f / %0.2f"," ", video.params.vsar, video.params.vpar);
        CSImGui::InfoRow("Signal Peak" , "%d", video.params.vsig_peak);
        CSImGui::InfoRow("Avg BPP", "%d", video.params.average_bpp);
        CSImGui::InfoRow("Bitrate", "%d kbps", video.stats.vbitrate / 1000);

        CSImGui::EndInfoTable();
    }
}

void ShowAudioInfo(WindowRuntime* runtime) {

    auto* player_session = runtime->resource.GetPlayerSession();
    if(!player_session) return;
    auto* state = player_session->GetState();
    if (!state) return;

    auto audio = state->GetAudioModel();

    // ====== Audio Devices ======
    if (!audio.device.audioDevices.empty()) {

        if (CSImGui::ModernCollapsingHeader("Audio Devices",ImGuiTreeNodeFlags_DefaultOpen)) {
            // 1. Định nghĩa cấu trúc bảng
            std::vector <TableCol> cols = {
                {"Device Name", 180.0f},
                {"Description", 0.0f},   // Stretch
                {"Status", 80.0f}
            };

            // 2. Bắt đầu bảng
            if (CSImGui::BeginListTable("audio_devices_v2", cols)) {
                
                for (auto& dev : audio.device.audioDevices) {
                    bool active = (dev.name == audio.device.audio_device);


                    CSImGui::BeginListRow("audio_devices_v2_row");

                    if (active) ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "● %s", dev.name.c_str());
                    else ImGui::Text("%s", dev.name.c_str());

                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", dev.description.empty() ? "N/A" : dev.description.c_str());

                    ImGui::TableNextColumn();
                    if (active) ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Active");
                    else ImGui::TextDisabled("Idle");

                    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
          

                    CSImGui::EndListRow();
                }
                CSImGui::EndListTable();
            }
        }
    }

    // ====== Audio Info ======
    CSImGui::ModernHeader("Audio Overview");

    if(CSImGui::BeginInfoTable("audio_info")){

        CSImGui::InfoRow("Audio Client", "%s", audio.device.audio_client_name.c_str());
        CSImGui::InfoRow("Audio Device", "%s", audio.device.audio_device.c_str());
        CSImGui::InfoRow("Audio Codec", "%s", audio.codec.acodec.c_str());
        CSImGui::InfoRow("Audio Format", "%s", audio.params.aformat.c_str());
        CSImGui::InfoRow("Audio OutPut", "%s", audio.codec.a_out.c_str());
        CSImGui::InfoRow("Audio Fillter", "%s", audio.codec.a_filter.c_str());


        CSImGui::EndInfoTable();
    }


    // ====== Audio Properties ======
    CSImGui::ModernHeader("Audio Properties");

    if(CSImGui::BeginInfoTable("audio_props")){

        CSImGui::InfoRow("Mute :" , "%s", audio.volume.isMuted ? "Yes" : "No") ;   
        CSImGui::InfoRow("Volume :", "%d%%", audio.volume.volume);
        CSImGui::InfoRow("Delay Audio :", "%.2f s", audio.codec.audio_delay);
        CSImGui::InfoRow("Bitrate Audio :", "%d kbps", audio.codec.abitrate / 1000);
        CSImGui::InfoRow("Sample Rate :", "%d Hz", audio.params.asamplerate);
        CSImGui::InfoRow("Channels :", "%s (%d)",audio.params.achannels_str.c_str(),audio.params.channel_count);
        CSImGui::InfoRow("Channels HR :", "%s", audio.params.ahr_channels.c_str());

        CSImGui::EndInfoTable();
    }
}
void ShowTrackInfo(WindowRuntime* runtime) {

    auto* player_session = runtime->resource.GetPlayerSession();
    if(!player_session) return;
    auto* state = player_session->GetState();
    if (!state) return;

    auto trackinfo = state->GetTrackModel();

    CSImGui::ModernHeader("Track List");

    for (const auto& track : trackinfo.tracks) {
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
void ShowPlaybackInfo(WindowRuntime* runtime) {

    auto* player_session = runtime->resource.GetPlayerSession();
    if(!player_session) return;
    auto* state = player_session->GetState();
    if (!state) return;

    auto playback = state->GetPlaybackModel();
    auto playlist = state->GetPlaylistModel();
    auto subtitle = state->GetSubtitleModel();
    auto network = state->GetNetworkModel();
    auto audio = state->GetAudioModel();

    // ====== STATE ======
    CSImGui::ModernHeader("Playback State");

    if(CSImGui::BeginInfoTable("playback_state")){

        CSImGui::InfoRow("Status :","%s", PlaybackStateToString(state->GetPlaybackState()));
        CSImGui::InfoRow("Video Type :", "%s", VideoTypeToString(state->GetVideoType()));
        CSImGui::InfoRow("Loop Mode :", "%s", playback.config.loopMode.c_str());

        CSImGui::InfoRow("Has File :", "%s", playlist.g_playlist_count > 0? "Yes" : "No");
        CSImGui::InfoRow("Seekable :", "%s", playback.flags.seekable ? "Yes" : "No");
        CSImGui::InfoRow("Idle :", "%s", playback.flags.isIdleActive ? "Yes" : "No");

        CSImGui::InfoRow("Sub Visible :", "%s", subtitle.sub_Visible ? "Yes" : "No");

        CSImGui::EndInfoTable();
    }


    // ====== TIMELINE ======

    CSImGui::ModernHeader("Timeline");

    float progress = 0.0f;
    if (playback.timing.duration > 0.0f)
        progress = (float)(playback.timing.timePos / playback.timing.duration);

    // Progress bar
    ImGui::ProgressBar(progress, ImVec2(-1, 8));
    
    if(CSImGui::BeginInfoTable("playback_time")){

        CSImGui::InfoRow("Current :", "%.2f / %.2f s", playback.timing.timePos, playback.timing.duration);
        CSImGui::InfoRow("Stream Pos :", "%d Bytes", network.stream_pos);
        CSImGui::InfoRow("Current :", "%.2f / %.2f s", playback.timing.timePos, playback.timing.duration);
        CSImGui::InfoRow("Remaining :", "%.2f s", playback.timing.time_remaining);
        CSImGui::InfoRow("Percent :", "%.2f%%", playback.timing.percent_pos);

        CSImGui::EndInfoTable();
    }


    // ====== PLAYBACK PROPERTIES ======
    CSImGui::ModernHeader("Properties");

    if(CSImGui::BeginInfoTable("playback_props")){

        CSImGui::InfoRow("Volume :", "%d%%", audio.volume.volume);
        CSImGui::InfoRow("Speed :", "%.2fx", playback.config.speed);
        CSImGui::InfoRow("Subtitle Delay :", "%.2f s", subtitle.sub_Delay);

        CSImGui::EndInfoTable();
    }
}

void ShowMetadata(WindowRuntime* runtime) {
    auto* player_session = runtime->resource.GetPlayerSession();
    if(!player_session) return;
    if(auto* state = player_session->GetState()) {
        state->ReadMedia([](auto const& m) {
            if (m.metadata.empty()) {
            ImGui::TextDisabled("No metadata available.");
            } else {
                for (auto& [key, value] : m.metadata) {
                    ImGui::TextWrapped("%s: %s", key.c_str(), value.c_str());
                }
            }

        });
    }
}
void ShowNetworkInfo(WindowRuntime* runtime) {

    auto* player_session = runtime->resource.GetPlayerSession();
    if(!player_session) return;
    auto* state = player_session->GetState();
    if (!state) return;

    auto network = state->GetNetworkModel();

    CSImGui::ModernHeader("Network / Buffer");

    if(CSImGui::BeginInfoTable("network_info")){

        
        CSImGui::InfoRow("Cache Buffer State:", "%d s", network.cache_buffering_state);
        CSImGui::InfoRow("Cache Duration :", "%.2f s", network.demuxer_cache_duration);
        CSImGui::InfoRow("Cache Time :", "%.2f s", network.demuxer_cache_time);
        CSImGui::InfoRow("Audio Buffer :", "%.2f s", network.audio_buffer);
        CSImGui::InfoRow("Bitrate :", "%.2f kbps", network.demuxer_bitrate);
        CSImGui::InfoRow("Via Network :", "%s", network.demuxer_via_network ? "Yes" : "No");

        CSImGui::EndInfoTable();
    }
    float bufferRatio = 0.0f;

    if (network.demuxer_cache_duration > 0.0f) {
        bufferRatio = network.demuxer_cache_time /
                    network.demuxer_cache_duration;
    }

    // Clamp tránh lỗi
    bufferRatio = std::clamp(bufferRatio, 0.0f, 1.0f);

    CSImGui::ModernHeader("Buffer");
    ImGui::ProgressBar(bufferRatio, ImVec2(-1, 8));

}
void ShowDuBugInFo(WindowRuntime* window){
       
    if (window) return;
    ImGui::TextWrapped("=== Debug Info ===");
    // WinAPI
    RECT rcWin, rcClient;

    //auto& 
    GetWindowRect(window->resource.hwnd, &rcWin);
    GetClientRect(window->resource.hwnd, &rcClient);
    POINT pt = { rcClient.left, rcClient.top };
    ClientToScreen(window->resource.hwnd, &pt);
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
    sdlFlags = SDL_GetWindowFlags(window->resource.sdlWindow);
    auto layout = window->properties.GetValue<WindowLayout>("Layout");

    ImGui::TextWrapped("SDL Client: %dx%d", layout.WinW, layout.WinH);
    ImGui::TextWrapped("SDL Position: X:%d Y:%d", layout.WinX, layout.WinY);
    
    ImGui::TextWrapped("SDL Flags: 0x%08X", sdlFlags);
    
    ImGui::TextWrapped("Client Pos: %dx%d", (int)layout.ClientPos.x,(int)layout.ClientPos.y);
    ImGui::TextWrapped("Client Size: %dx%d", (int)layout.ClientSize.x, (int)layout.ClientSize.y);

    ImGui::Separator();
    ImGui::TextWrapped("IsMaximized: %s", window->state.display.isMaximized ? "Yes" : "No");
    ImGui::TextWrapped("IsFullscreen: %s", window->state.display.isFullscreen ? "Yes" : "No");
    ImGui::Separator();
    ImGui::TextWrapped("HitTest Zone: %s", window->state.input.hittestname.c_str());
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
void ShowVideoInfoPopup(WindowRuntime* runtime, bool& closePopup_VideoInFo) {
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
                {"Media",    [&]() { ShowMediaInfo(runtime); }},
                {"Video",    [&]() { ShowVideoInfo(runtime); }},
                {"Audio",    [&]() { ShowAudioInfo(runtime); }},
                {"Playback", [&]() { ShowPlaybackInfo(runtime); }},
                {"Network",  [&]() { ShowNetworkInfo(runtime); }},
                {"Track",    [&]() { ShowTrackInfo(runtime); }},
                {"Metadata", [&]() { ShowMetadata(runtime); }}
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
            bool showDebug = true;
            if (showDebug && CSImGui::ModernTabItem("Debug")) {
                if(CSImGui::BeginCard()){
                    ShowDuBugInFo(runtime);
                    CSImGui::EndCard();
                }
                CSImGui::EndModernTabItem();
            }

            CSImGui::EndModernTabBar();
        }
        
    //    EndModernChild();
    //}
}
void OpenVideoInfoPopup(ReusablePopup& popup) {
    popup.Open("Video Info", [](WindowRuntime* runtime, bool& closePopup_VideoInFo) {
        ShowVideoInfoPopup(runtime, closePopup_VideoInFo);  // truyền ref
    });
}

void RenderVideoInfoPopup(ReusablePopup& popup, WindowRuntime* window){
    // Render popup mỗi frame
    if(popup.IsOpen()) {
        popup.Render(window);
    }
}
