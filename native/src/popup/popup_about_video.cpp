#include "reusable_popup.h"
#include"globals.h"
#include"utils.h"

#include "mpv/mpv_basic_formats.h"

#include "mpv/scripts/script_manager.h"

#include"popup_about_video.h"

#include"windows/windows_borderless_state.h"

#include <imgui.h>

// ------------ Các hàm hiển thị nội dung riêng -------------
void ShowMediaInfo() {

    if(g_playbackStatus.Title)                  ImGui::TextWrapped("Title: %s", g_playbackStatus.Title);
    if(g_playbackStatus.mediaTitle)             ImGui::TextWrapped("MediaTitle: %s", g_playbackStatus.mediaTitle);
    if(g_playbackStatus.filename)               ImGui::TextWrapped("File: %s", g_playbackStatus.filename);
    if(g_playbackStatus.streamUrl)              ImGui::TextWrapped("Stream URL: %s", g_playbackStatus.streamUrl);
    if(g_playbackStatus.fileFormat)             ImGui::TextWrapped("File format: %s", g_playbackStatus.fileFormat);
}

void ShowVideoInfo() {
    ImGui::TextWrapped("Video Light Name :%s |", g_videoInfo.g_videoparams.vlight.c_str());
    ImGui::TextWrapped("Video Realistic FPS :%.2f |" , g_videoInfo.currentFPS);ImGui::SameLine();
    ImGui::TextWrapped("Video Estimated_vf_fps_mpv :%.2f |", g_videoInfo.estimated_vf_fps_mpv);
    ImGui::TextWrapped("Video Min FPS :%.2f / Video Max FPS: %.2f |", g_videoInfo.minFPS, g_videoInfo.maxFPS);ImGui::Separator();
    ImGui::TextWrapped("Video Resolution :%dx%d |", g_videoInfo.g_videoparams.vwidth, g_videoInfo.g_videoparams.vheight);ImGui::SameLine();
    ImGui::TextWrapped("Video Resolution Ratio :%dx%d |", g_videoInfo.g_videoparams.vdisp_w, g_videoInfo.g_videoparams.vdisp_h);ImGui::Separator();
    ImGui::TextWrapped("Video Ratio :%s |", g_videoInfo.g_videoparams.vaspect_name.c_str());ImGui::SameLine();
    ImGui::TextWrapped("Video Ratio Sar:%s |", g_videoInfo.g_videoparams.vsar_name.c_str());ImGui::Separator();
    ImGui::TextWrapped("Video Crop X/Y :%dx%d |", g_videoInfo.g_videoparams.vcrop_x, g_videoInfo.g_videoparams.vcrop_y);ImGui::SameLine();
    ImGui::TextWrapped("Video Crop W/H :%dx%d |", g_videoInfo.g_videoparams.vcrop_w, g_videoInfo.g_videoparams.vcrop_h);ImGui::Separator();
    ImGui::TextWrapped("Video Aspect ratio :%.2f |", g_videoInfo.g_videoparams.vaspect);
    ImGui::TextWrapped("Video codec :%s |", g_videoInfo.vcodec);
    ImGui::TextWrapped("Video Pixel :%s |", g_videoInfo.g_videoparams.vpixfmt.c_str());
    ImGui::TextWrapped("Video HW Decoding :%s |", g_videoInfo.hwdec);ImGui::SameLine();
    ImGui::TextWrapped("Video Format :%s |", g_videoInfo.video_format);ImGui::Separator();
    ImGui::TextWrapped("Video Primaries :%s |", g_videoInfo.g_videoparams.vprimaries.c_str());ImGui::SameLine();
    ImGui::TextWrapped("Video Gamma :%s |", g_videoInfo.g_videoparams.vgamma.c_str());ImGui::SameLine();
    ImGui::TextWrapped("Video ColorMatrix :%s |", g_videoInfo.g_videoparams.vcolormatrix.c_str());ImGui::SameLine();
    ImGui::TextWrapped("Video Colorlevels :%s |", g_videoInfo.g_videoparams.vcolorlevels.c_str());ImGui::Separator();
    ImGui::TextWrapped("Video Stereo in :%s |", g_videoInfo.g_videoparams.vstereo_in.c_str());
    ImGui::TextWrapped("Video Chroma_location : %s |", g_videoInfo.g_videoparams.vchroma_location.c_str()); 
    ImGui::TextWrapped("Video sar  : %d |", g_videoInfo.g_videoparams.vsar); ImGui::SameLine();
    ImGui::TextWrapped("Video par  : %d |", g_videoInfo.g_videoparams.vpar); ImGui::SameLine();
    ImGui::TextWrapped("Video Sig peak : %d |", g_videoInfo.g_videoparams.vsig_peak); 
    ImGui::TextWrapped("Video Average bpp : %d |", g_videoInfo.g_videoparams.average_bpp); 
    ImGui::TextWrapped("Video Video bitrate : %d kbps |", g_videoInfo.vbitrate / 1000); 
}

void ShowAudioInfo() {
    // ---------------- Audio Devices ----------------
    if(g_audioDevices.size() >= 1){
        if (ImGui::CollapsingHeader("Audio Devices")) {
            ImGui::Columns(3, "audio_devices", true);
            ImGui::TextWrapped("Name"); ImGui::NextColumn();
            ImGui::TextWrapped("Description"); ImGui::NextColumn();
            ImGui::TextWrapped("Active"); ImGui::NextColumn();
            ImGui::Separator();

            for (auto& dev : g_audioDevices) {
                ImGui::TextWrapped("%s", dev.name.c_str()); ImGui::NextColumn();
                ImGui::TextWrapped("%s", dev.description.c_str()); ImGui::NextColumn();
                ImGui::TextWrapped("%s", (dev.name == std::string(g_videoInfo.audio_device)) ? "Yes" : "No"); ImGui::NextColumn();
            }

            ImGui::Columns(1);
        }
    }
    ImGui::TextWrapped("Audio Client: %s |", g_playbackStatus.audio_client_name);
    ImGui::TextWrapped("Audio device: %s |", g_videoInfo.audio_device);
    ImGui::TextWrapped("Audio codec: %s |", g_videoInfo.acodec);ImGui::SameLine();
    ImGui::TextWrapped("Audio format: %s |", g_videoInfo.g_audioarams.aformat.c_str());
    ImGui::TextWrapped("Audio Delay: %.2f s |", g_videoInfo.audio_delay );
    ImGui::TextWrapped("Audio bitrate: %d kbps |", g_videoInfo.abitrate / 1000);ImGui::SameLine();
    ImGui::TextWrapped("Audio Sample rate: %d Hz |", g_videoInfo.g_audioarams.asamplerate);
    ImGui::TextWrapped("Audio channels: %d |", g_videoInfo.g_audioarams.channel_count);ImGui::SameLine();
    ImGui::TextWrapped("Audio channels_str: %s |", g_videoInfo.g_audioarams.achannels_str.c_str());
    ImGui::TextWrapped("Audio ahr_channels: %s |", g_videoInfo.g_audioarams.ahr_channels.c_str());
    ImGui::TextWrapped("Audio Output: %s |", g_videoInfo.a_out);
    ImGui::TextWrapped("Audio Filter: %s |", g_videoInfo.a_filter);
}
void ShowTrackInfo() {
    ImGui::TextUnformatted("🎵 Track List");
    ImGui::Separator();

    for (const auto& track : g_videoInfo.g_tracks) {

        // Hàng chính (tóm tắt)
        ImGui::Text("[%s #%d]", track.type.c_str(), track.id);
        ImGui::SameLine();
        ImGui::Text("%s", track.codec.c_str());
        if (!track.codec_profile.empty()) {
            ImGui::SameLine();
            ImGui::Text("(%s)", track.codec_profile.c_str());
        }

        if (track.selected) { ImGui::SameLine(); ImGui::Text(" Selected"); }
        if (track.is_default) { ImGui::SameLine(); ImGui::Text(" Default"); }

        // Mở rộng để xem chi tiết
        if (ImGui::TreeNode((std::string("Details##") + track.type + std::to_string(track.id)).c_str() , "Details")) {
            ImGui::Indent(10.0f);
            ImGui::Text("Codec Desc: %s", track.codec_desc.c_str());
            ImGui::Text("Decoder: %s", track.decoder.c_str());
            ImGui::Text("Decoder Desc: %s", track.decoder_desc.c_str());
            ImGui::Text("Format: %s", track.format_name.c_str());
            ImGui::Text("Lang: %s", track.language.c_str());
            ImGui::Text("Title: %s", track.title.c_str());
            ImGui::Text("External: %s", track.external ? "Yes" : "No");
            ImGui::Text("Dependent: %s", track.dependent ? "Yes" : "No");
            ImGui::Text("Forced: %s", track.forced ? "Yes" : "No");

            if (track.type == "video") {
                ImGui::Text("Resolution: %dx%d", track.demux_w, track.demux_h);
                ImGui::Text("FPS: %.2f", track.demux_fps);
            }
            else if (track.type == "audio") {
                ImGui::Text("Sample Rate: %d Hz", track.demux_samplerate);
                ImGui::Text("Channels: %s (%dch)", track.demux_channels.c_str(), track.demux_channel_count);
            }

            ImGui::Unindent(10.0f);
            ImGui::TreePop();
        }
        // khoảng cách giữa các track
        ImGui::Spacing();
    }
}
void ShowPlaybackInfo() {
    ImGui::TextWrapped("Playback status: %s", PlaybackStateToString(GetPlaybackState()));
    ImGui::TextWrapped("Video Type: %s", VideoTypeToString(GetVideoType()));
    ImGui::TextWrapped("SubVisible: %s", g_playbackStatus.g_subinfo.sub_Visible ? "Yes" : "No");
    ImGui::TextWrapped("Idle Active: %s", g_playbackStatus.idle_active ? "Yes" : "No");
    ImGui::TextWrapped("Seekable: %s", g_playbackStatus.seekable ? "Yes" : "No");
    ImGui::TextWrapped("File: %s", g_playbackStatus.hasFile ? "Yes" : "No");
    ImGui::TextWrapped("SubDelay time: %.2f s", g_playbackStatus.g_subinfo.sub_Delay);
    ImGui::TextWrapped("Current time: %.2f s / %.2f s", g_playbackStatus.timePos, g_playbackStatus.duration);
    ImGui::TextWrapped("Time Remaining : %.2f s", g_playbackStatus.time_remaining);
    ImGui::TextWrapped("Percent Pos : %.2f s", g_playbackStatus.percent_pos);
    ImGui::TextWrapped("Speed: %.2fx", g_playbackStatus.speed);
    ImGui::TextWrapped("Loop mode: %s", g_playbackStatus.loopMode);
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
    ImGui::TextWrapped("Demuxer Cache Duration: %.2f s", g_playbackStatus.demuxer_cache_duration );
    ImGui::TextWrapped("Demuxer Cache Time: %.2f s", g_playbackStatus.demuxer_cache_time);
    ImGui::TextWrapped("Audio Buffer time Video: %.2f s", g_playbackStatus.audio_buffer);
    ImGui::TextWrapped("Demuxer Ditrate: %.2f s", g_playbackStatus.demuxer_bitrate);
    ImGui::TextWrapped("Network : %s", g_playbackStatus.demuxer_via_network ? "Yes" : "No");
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
    ImGui::TextWrapped("SDL Draw: %dx%d", Windowlayout.DrawWinW, Windowlayout.DrawWinH);
    ImGui::TextWrapped("SDL Client: %dx%d", Windowlayout.WinW, Windowlayout.WinH);
    ImGui::TextWrapped("SDL Position: X:%d Y:%d", Windowlayout.WinX, Windowlayout.WinY);
    
    ImGui::TextWrapped("SDL DisplayDPI: %d", (int)Windowlayout.DisplayDPI);
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
        if (ImGui::BeginTabItem("Media"))   {ShowMediaInfo();ImGui::EndTabItem();}
        if (ImGui::BeginTabItem("Video"))   {ShowVideoInfo();ImGui::EndTabItem();}
        if (ImGui::BeginTabItem("Audio"))   {ShowAudioInfo();ImGui::EndTabItem();}
        if (ImGui::BeginTabItem("Playback")){ShowPlaybackInfo();ImGui::EndTabItem();}
        if (ImGui::BeginTabItem("Network")) {ShowNetworkInfo();ImGui::EndTabItem();}
        if (ImGui::BeginTabItem("Track"))   {ShowTrackInfo();ImGui::EndTabItem();}
        if (ImGui::BeginTabItem("Metadata"))   {ShowMetadata();ImGui::EndTabItem();}
        
        if (g_DragResizeState.showDebug) {if (ImGui::BeginTabItem("DeBug")) {ShowDuBugInFo();ImGui::EndTabItem();}}
            
        ImGui::PopStyleColor();
        ImGui::EndTabBar();
    }

    ImGui::Separator();

    if (ImGui::Button("Close")) {
        closePopup_VideoInFo = true;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
}
void RenderVideoInfoPopup(){
    // Render popup mỗi frame
    videoInfoPopup.Render();
}
