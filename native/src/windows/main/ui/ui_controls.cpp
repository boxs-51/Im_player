#include "ui_controls.h"
#include "ui_settings.h"
#include "player/mpv_data.h"
#include "player/mpv_basic_formats.h"
#include "player/session/PlayerSession.h"
#include "gui/gui.h"
#include "popup/popup.h"
#include "settings_manager.h"
#include "utils.h"
#include "WindowRuntime.h"
#include <SDL.h>
#include <algorithm>

static int g_lastVolumeBeforeMute = 50;
static Uint64 volumeSliderVisibleUntil = 0;
static bool showSettings = false;
static bool showOptionMenu = false;
static bool header_hoverd = false;
static bool Volume_action = false;
static bool volume_hover = false;
static bool seek_bar_action = false;
static bool seek_bar_hover = false;
static bool items_action = false;
static bool items_hover = false;
static bool was_ui_video = false;

void DrawTimeDisplay(double current, double duration, ImVec2& videoSize, ImVec2& pos, float parentHeight) {
    float baseFontSizeMultiplier = 2.0f;
    float dynamicScale = videoSize.x / 1920.0f * baseFontSizeMultiplier;
    dynamicScale = std::max(dynamicScale, 0.5f);

    int curH = (int)(current / 3600), curM = (int)(current / 60) % 60, curS = (int)current % 60;
    int durH = (int)(duration / 3600), durM = (int)(duration / 60) % 60, durS = (int)duration % 60;

    char timeStr[64];
    if (durH > 0) {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d / %02d:%02d:%02d", curH, curM, curS, durH, durM, durS);
    } else {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d / %02d:%02d", curM, curS, durM, durS);
    }

    float oldFontScale = ImGui::GetFont()->Scale; 
    ImGui::GetFont()->Scale = dynamicScale;
    ImGui::PushFont(ImGui::GetFont()); 

    ImVec2 textSize = ImGui::CalcTextSize(timeStr);
    float centeredY = pos.y + (parentHeight - textSize.y) * 0.5f;

    ImGui::SetCursorPos(ImVec2(pos.x, centeredY));
    ImGui::TextUnformatted(timeStr);

    ImGui::PopFont();
    ImGui::GetFont()->Scale = oldFontScale;
}

void RenderPlayerControls(WindowRuntime* runtime, const ImVec2& _pos, const ImVec2& _size) {  
    bool show_ui_video = runtime->properties.GetValue<bool>("ShowUiVideo", true);
    if(!show_ui_video)
        showSettings = false;

    static float uiAlpha = 0.0f;

    UpdateHoverAnim(uiAlpha, show_ui_video, 5.0f);
    was_ui_video = true;

    if (uiAlpha <= 0.01f){
        was_ui_video = false;
        return; 
    }

    auto& Cfg = ConfigManager::Instance();
    
    ImVec2 videoPos = (_pos);
    ImVec2 videoSize = (_size);
    MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
    VideoInfo& g_videoInfo = GetVideoInfo();
    bool paused = g_playbackStatus.isPaused;
    float playbackTime = (float)g_playbackStatus.playbackTime;
    float time_show_ui = (float)g_playbackStatus.timePos;
    float duration = (float)g_playbackStatus.duration;
    int volume = (int)g_playbackStatus.volume;
    bool isMuted = g_playbackStatus.isMuted;

    float timePosX = 0.0f;
    float scale = std::clamp(videoSize.x / 1280.0f, 0.1f, 2.0f);
    float spacing = videoSize.x * 0.05f;
    ImVec2 iconSize(35 * scale, 35 * scale);
    const float minWidthForControls = 750.0f;
    const float minWHegthForControls = 450.0f;
    bool onlyShowSeekBar = (videoSize.x <= minWidthForControls || videoSize.y <= minWHegthForControls);

    ImVec2 Pos = ImVec2(videoPos.x, videoPos.y);
    ImVec2 Possize = ImVec2(videoSize.x, videoSize.y);

    float sliderWidth = videoSize.x * 0.95f;
    ImVec2 controlPosBar((videoSize.x - sliderWidth) * 0.5f, videoSize.y * 0.85f);
    ImVec2 controlPos(videoSize.x * 0.05f, videoSize.y * 0.90f);

    if (onlyShowSeekBar) {
        sliderWidth = videoSize.x;
        controlPosBar = ImVec2(0.0f, videoSize.y - (videoSize.y * 0.025f));
    }

    ImGui::SetNextWindowPos(Pos);
    //ImGui::SetNextWindowSize(Possize);
    ImGui::SetNextWindowBgAlpha(0.20f * uiAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, uiAlpha);
    ImGui::BeginChild("##Controls",Possize);
    
    // 1. Cấu hình thông số
    const int PADDING_HEADER = 10;
    float button_width = 50.0f;
    float gap_between = 10.0f;
    float internal_margin = 0.0f;

    ImVec2 header_pos = ImVec2(PADDING_HEADER, PADDING_HEADER);
    float total_w = videoSize.x - 2 * PADDING_HEADER;
    float total_h = ImGui::GetTextLineHeightWithSpacing() + 10.0f;

    // Đặt Cursor để bắt đầu vẽ các Item
    ImGui::SetCursorPos(header_pos);
    ImVec2 p_start = ImGui::GetCursorScreenPos();

    // --- ITEM 1: PHẦN TEXT (Dùng InvisibleButton để lấy logic Item) ---
    float text_bg_w = total_w - button_width - gap_between;
    ImGui::PushID("Header_Text_Part");
    ImGui::InvisibleButton("##TextPart", ImVec2(text_bg_w, total_h));
    bool is_text_hovered = ImGui::IsItemHovered();
    bool is_text_active = ImGui::IsItemActive();
    if (ImGui::IsItemClicked()) { /* Xử lý click vào tiêu đề */ }
    //CSImGui::ShowTooltipDelayed(g_playbackStatus.mediaTitle.empty() ? "No Title" : g_playbackStatus.mediaTitle.c_str(), is_text_hovered , 3.0, "Header_Text_Part");
    CSImGui::ToolTip(g_playbackStatus.mediaTitle.empty() ? "No Title" : g_playbackStatus.mediaTitle.c_str() , 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);
    ImGui::PopID();

    //--- ITEM 2: PHẦN BUTTON (Nằm cùng dòng) ---
    ImGui::SameLine(0, gap_between); 
    ImGui::PushID("Header_Btn_Part");
    ImGui::InvisibleButton("##BtnPart", ImVec2(button_width, total_h));
    bool is_btn_hovered = ImGui::IsItemHovered();
    bool is_btn_active = ImGui::IsItemActive();
    header_hoverd = is_btn_hovered||is_text_hovered;
    if (ImGui::IsItemClicked()) { /* Xử lý click vào nút CLOSE */ }
    ImGui::PopID();

    // --- PHẦN VẼ (DRAWING) ---
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float rounding = total_h * 0.5f;

    // Logic Animation (Dựa trên trạng thái Item)
    static float uiAlpha_text = 0.0f;
    static float uiAlpha_btn = 0.0f;
    UpdateHoverAnim(uiAlpha_text, is_text_hovered, 5.0f);
    UpdateHoverAnim(uiAlpha_btn, is_btn_hovered, 8.0f);

    // Vẽ Background Text
    ImVec2 t_min = p_start;
    ImVec2 t_max = ImVec2(p_start.x + text_bg_w, p_start.y + total_h);
    ImU32 t_color = is_text_active ? IM_COL32(255, 255, 255, 65) : IM_COL32(255, 255, 255, 20 + (int)(25 * uiAlpha_text));
    draw_list->AddRectFilled(t_min, t_max, t_color, rounding);

    // Vẽ Background Button
    ImVec2 b_min = ImVec2(t_max.x + gap_between, p_start.y);
    ImVec2 b_max = ImVec2(b_min.x + button_width, p_start.y + total_h);
    // Nhấn xuống (active) thì màu sang hơn
    ImU32 b_color = is_btn_active ? IM_COL32(255, 255, 255, 65) : IM_COL32(255, 255, 255, 20 + (int)(25 * uiAlpha_btn));
    draw_list->AddRectFilled(b_min, b_max, b_color, rounding);

    // --- VẼ NỘI DUNG ---
    // Text Title
    const char* title = (!g_playbackStatus.mediaTitle.empty()) ? g_playbackStatus.mediaTitle.c_str() : "No Title";
    ImVec2 text_pos = ImVec2(t_min.x + 15.0f, t_min.y + (total_h - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::RenderTextEllipsis(draw_list, text_pos, ImVec2(t_max.x - 10.0f, t_max.y), t_max.x - 10.0f, title, NULL, NULL);

    // Text Button
    const char* btn_label = "X";
    ImVec2 l_size = ImGui::CalcTextSize(btn_label);
    draw_list->AddText(ImVec2(b_min.x + (button_width - l_size.x) * 0.5f, b_min.y + (total_h - l_size.y) * 0.5f), IM_COL32(255, 255, 255, 255), btn_label);

    // Đặt Cursor xuống dưới phần nền để vẽ các thành phần khác của UI
    //ImGui::SetCursorScreenPos(ImVec2(p_min.x, p_max.y + 5.0f));

    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();

    draw_list->AddRect(win_pos, ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y), IM_COL32(0,0,0,100), 0.0f, 0, 4.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1, 1, 1, 0.1f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1, 1, 1, 0.2f));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 1.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

    float value = playbackTime;
    float v_min = 0.0f;
    float v_max = duration;

    SliderFlags flags = SliderFlags_Default | SliderFlags_EnableSmoothPreview | SliderFlags_Animation;

    ImGui::SetCursorPos(ImVec2(controlPosBar));
    
    CSImGui::ModernSliderFloatEx(
        "##SeekBar",
        &value,
        v_min,
        v_max,
        6.0f * scale,
        8.0f * scale,
        "%.2f",
        sliderWidth,
        flags,
        [&](Phase phase, Slot slot, SliderState* state, SliderRenderData* rd, ImDrawList* dl)
        {
            if(phase == Phase::Init){
                rd->buffer_t = g_playbackStatus.demuxer_cache_time / duration;

                int currentChapterIndex = -1;
                double start_time = 0.0f;
                double end_time = 0.0f;
                if (!g_videoInfo.g_chapters.empty()) {
                    for (int i = 0; i < (int)g_videoInfo.g_chapters.size(); i++) {
                        start_time = g_videoInfo.g_chapters[i].time;
                        end_time = (i + 1 < (int)g_videoInfo.g_chapters.size()) ? g_videoInfo.g_chapters[i+1].time : g_playbackStatus.duration;
                        if (g_playbackStatus.playbackTime >= start_time && g_playbackStatus.playbackTime < end_time) {
                            currentChapterIndex = i;
                            break;
                        }
                    }
                }
                if (currentChapterIndex >= 0 && duration > 0.0)
                {
                    float start_t = (float)(start_time / duration);
                    float end_t = (float)(end_time / duration);

                    rd->chapter_range_start = ImClamp(start_t, 0.0f, 1.0f);
                    rd->chapter_range_end = ImClamp(end_t, 0.0f, 1.0f);
                }
                rd->markers.clear();
                rd->markers.reserve(g_videoInfo.g_chapters.size());
                for (auto& c : g_videoInfo.g_chapters)
                {
                    rd->markers.push_back(c.time / duration);
                }

                rd->col_track = ImVec4(0.235f, 0.235f, 0.235f, 0.706f);
                rd->col_buffer = ImVec4(0.784f, 0.784f, 0.784f, 0.588f);
                rd->col_fill = ImVec4(1.000f, 0.235f, 0.235f, 0.863f);
                rd->col_grab = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
                rd->col_marker = ImVec4(1.000f, 0.784f, 0.000f, 0.784f);
                rd->col_grab_border = ImVec4(0.000f, 0.000f, 0.000f, 0.784f);
                rd->col_chapter_range = ImVec4(0.392f, 0.392f, 1.000f, 0.235f);
                rd->col_border = ImVec4(0.300f, 0.300f, 0.300f, 1.000f);
                rd->col_grab_shadow = ImVec4(0.000f, 0.000f, 0.000f, 0.350f);
                if(rd->active){
                    rd->col_fill = ImVec4(1.00f, 0.10f, 0.10f, 1.00f);
                    rd->col_grab = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
                }else if(rd->grab_hovered){
                    rd->col_fill = ImVec4(1.00f, 0.30f, 0.30f, 0.90f);
                    rd->col_grab = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
                }else if(rd->bar_hovered){
                    rd->col_track = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
                }
                rd->height_on_hover = rd->height * 0.5f;
                float target_alpha = (rd->hovered || rd->active) ? 1.0f : 0.0f;
                float speed = 12.0f;
                static float smooth_grab_alpha = 0.0f;
                smooth_grab_alpha = ImLerp(smooth_grab_alpha, target_alpha, SMOOTH_LERP(speed, state->dt));
                
                rd->col_grab.w *= smooth_grab_alpha;
                rd->col_grab_border.w *= smooth_grab_alpha;
                rd->col_grab_shadow.w *= smooth_grab_alpha;
                if (!rd->hovered && !rd->active) {
                    rd->grab_radius = 0.0f;
                }
            }
        },
        [&](Phase phase, Slot slot, TooltipData* td, ImDrawList* dl)
        {
            if (phase == Phase::Init)
            {
                int sec = (int)td->item.seek_value;
                int h = sec / 3600;
                int m = (sec % 3600) / 60;
                int s = sec % 60;

                if (td->item.seek_value >= 0 && td->item.seek_value <= duration)
                {
                    if (h > 0)
                        td->config.text = Format("%d:%02d:%02d", h, m, s);
                    else
                        td->config.text = Format("%02d:%02d", m, s);
                }
                if (!g_videoInfo.g_chapters.empty()) {
                    td->config.show_title = false;

                    for (size_t i = 0; i < g_videoInfo.g_chapters.size(); ++i) {
                        double startTime = g_videoInfo.g_chapters[i].time;
                        double endTime = (i + 1 < g_videoInfo.g_chapters.size()) 
                                        ? g_videoInfo.g_chapters[i + 1].time 
                                        : g_playbackStatus.duration; 

                        if (td->item.seek_value >= startTime && td->item.seek_value < endTime) {
                            td->config.show_title = true;
                            td->config.title = g_videoInfo.g_chapters[i].title.c_str();
                            break; 
                        }
                    }
                }
                td->config.max_width = 240.0f;
                td->config.max_height = 180.0f;
                td->config.align = ImGuiTooltip::Align_Center;
            }
        },
        [&](const SliderSeekRequest* req)
        {
            SliderSeekResult res{};
            if ((req->from_drag || req->from_click) && !req->is_final)
            {
                res.accept = false;
            }
            else
            {
                if(req->is_hovered){
                    auto* playercommand = runtime->resource.GetPlayerSession()->GetCommander();
                    if (playercommand) 
                        playercommand->Seek(req->new_value, duration);
                    res.value = req->new_value;
                    res.accept = true;
                }
            }
            return res;
        },
        nullptr
    );
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    if(!onlyShowSeekBar){
        float i = 0.0f;
        auto* playercommand = runtime->resource.GetPlayerSession()->GetCommander();

        if(!(g_playbackStatus.g_PlayingIndex == 0 && g_playbackStatus.g_playlist_count > 0)){
            ImGui::SetCursorPos(ImVec2(controlPos.x, controlPos.y)); i = i + 1.0f;
            if (CSImGui::CustomIconButton("##prev", DrawPrevIcon, iconSize)) {
                if (playercommand) 
                    playercommand->PlaylistPrev();
            }
            CSImGui::ToolTip("Previous Video", 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);
        }

        static PlayPauseData playData;
        playData.paused = !paused;
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * i, controlPos.y)); i = i + 1.0f;
        if (CSImGui::CustomIconButton("##toggle", DrawPlayPauseIcon, iconSize, &playData)) {
            if (playercommand) {
                if (paused) playercommand->Play();
                else playercommand->Pause();
            }
        }
        CSImGui::ToolTip(paused ? "Play##Btntoggle" : "Pause##Btntoggle", 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);

        if(!(g_playbackStatus.g_PlayingIndex == (int)g_playbackStatus.g_playlist.size() - 1) && g_playbackStatus.g_playlist_count >= 2){
            ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * i, controlPos.y)); i = i + 2.0f;
            if (CSImGui::CustomIconButton("##next", DrawNextIcon, iconSize)) {
                if (playercommand) 
                    playercommand->PlaylistNext();
            }
            CSImGui::ToolTip("Next Video", 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);
        }

        ImGui::BeginGroup();
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * i, controlPos.y)); 
        
        static VolumeIconData volData;
        volData.volume = volume;
        volData.isMuted = isMuted;

        CSImGui::CustomIconButton("##volume", DrawVolumeIcon, iconSize, &volData);

        bool clicked = ImGui::IsItemClicked();
        bool hoverIcon = ImGui::IsItemHovered();

        CSImGui::ToolTip(isMuted ? "Unmute##BtnVol" : "Mute##BtnVol", 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);

        if (clicked) {
            if (playercommand) {
                if (isMuted || volume == 0) {
                    playercommand->SetMute(false);
                    playercommand->SetVolume(g_lastVolumeBeforeMute);
                } else {
                    g_lastVolumeBeforeMute = volume;
                    playercommand->SetVolume(0);
                    playercommand->SetMute(true);
                }
            }
        }

        bool showSlider = SDL_GetTicks64() < volumeSliderVisibleUntil;
        static float easedAnim = 0.0f;
        UpdateHoverAnim(easedAnim, showSlider, 10.0f);

        ImVec2 baseSize(100.0f * scale, iconSize.y * 0.5f);
        ImVec2 sliderSize(baseSize.x * easedAnim, baseSize.y); i = i + 0.85f;

        float slideOffset = ImLerp(-40.0f, 0.0f, easedAnim);
        ImVec2 sliderPos(controlPos.x + spacing * i + slideOffset,
                        controlPos.y + (iconSize.y - sliderSize.y) * 0.5f);

        ImVec2 mouseScreen = ImGui::GetIO().MousePos;
        ImVec2 localMouse = mouseScreen - ImGui::GetWindowPos();

        static bool hoverSlider = false;
        if (showSlider) {
            hoverSlider =
                localMouse.x >= sliderPos.x && localMouse.x <= (sliderPos.x + sliderSize.x) &&
                localMouse.y >= sliderPos.y && localMouse.y <= (sliderPos.y + sliderSize.y);
        }
        static bool knobActive = false;
        static bool knobHovered = false;
        static bool isHovered_Slider = false;
        static bool isActive_Slider = false;

        if (hoverIcon || hoverSlider || knobHovered || knobActive || isActive_Slider)
            volumeSliderVisibleUntil = SDL_GetTicks64() + 1000;

        if (showSlider) {
            ImGui::SameLine();
            ImGui::SetCursorPos(sliderPos);
            ImGui::SetNextItemWidth(sliderSize.x);

            ImGui::PushID("VolumeSlider");
            ImGui::InvisibleButton("##volume_slider_invisible", sliderSize);

            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
            isHovered_Slider = ImGui::IsItemHovered();
            isActive_Slider = ImGui::IsItemActive();

            static float hoverAnim = 0.0f;
            static float hoverAnimknob = 0.0f;

            UpdateHoverAnim(hoverAnim, (isHovered_Slider || knobHovered || isActive_Slider), 12.0f);

            if (isActive_Slider) {
                float newVolume = (localMouse.x - sliderPos.x) / sliderSize.x;
                volume = std::clamp(int(newVolume * 130.0f), 0, 130);
                if (playercommand) {
                    playercommand->SetVolume(volume);
                    if (volume > 0 && isMuted)
                        playercommand->SetMute(false);
                    if (volume > 0)
                        g_lastVolumeBeforeMute = volume;
                }

                Cfg.UpdateVideoSettings([volume](AppSettings& s){
                    s.defaultVolume = volume;
                });
                Cfg.SaveVideo();
            }

            ImGui::PopStyleColor(2);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 winPos = ImGui::GetWindowPos(); 

            auto LerpColor = [](ImU32 a, ImU32 b, float t) {
                ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
                ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
                ImVec4 c = ImVec4(
                    ca.x + (cb.x - ca.x) * t,
                    ca.y + (cb.y - ca.y) * t,
                    ca.z + (cb.z - ca.z) * t,
                    ca.w + (cb.w - ca.w) * t
                );
                return ImGui::ColorConvertFloat4ToU32(c);
            };

            ImU32 bgBase = IM_COL32(80, 80, 80, 180);
            ImU32 bgHover = IM_COL32(120, 120, 120, 220);
            ImU32 fillBase = IM_COL32(80, 160, 255, 255);
            ImU32 fillHover = IM_COL32(120, 200, 255, 255);

            ImU32 bgColor = LerpColor(bgBase, bgHover, hoverAnim);
            ImU32 fillColor = LerpColor(fillBase, fillHover, hoverAnim);

            float barHalf = sliderSize.y * (0.25f + hoverAnim * 0.1f);
            float filledWidth = sliderSize.x * (volume / 130.0f);
            float centerY = sliderPos.y + sliderSize.y * 0.5f;
            
            ImVec2 barStart(sliderPos.x + winPos.x, centerY - barHalf + winPos.y);
            ImVec2 barEnd(sliderPos.x + sliderSize.x + winPos.x, centerY + barHalf + winPos.y);
            ImVec2 filledEnd(sliderPos.x + filledWidth + winPos.x, centerY + barHalf + winPos.y);

            draw_list->AddRectFilled(barStart, barEnd, bgColor, sliderSize.y * 0.3f);
            draw_list->AddRectFilled(barStart, filledEnd, fillColor, sliderSize.y * 0.3f);

            float radius = (scale * 15.0f) * (1 + (hoverAnimknob * 0.1f));

            ImVec2 knobCenter(sliderPos.x + filledWidth + winPos.x, centerY + winPos.y);
            ImVec2 knobSize = ImVec2(radius * 2, radius * 2);
            ImVec2 knobTopLeft = ImVec2(knobCenter.x - radius - winPos.x, knobCenter.y - radius - winPos.y);

            ImGui::SetCursorPos(knobTopLeft);
            ImGui::InvisibleButton("##volume_knob_drag", knobSize);

            knobActive = ImGui::IsItemActive();
            knobHovered = ImGui::IsItemHovered();
            Volume_action = knobActive || isActive_Slider;
            volume_hover = knobHovered || isHovered_Slider || knobActive;
            UpdateHoverAnim(hoverAnimknob, (knobHovered || isActive_Slider || knobActive), 12.0f);

            ImU32 knobColorBase = IM_COL32(255, 100, 100, 255);
            ImU32 knobColorHover = IM_COL32(255, 170, 170, 255);

            ImU32 knobColor = LerpColor(knobColorBase, knobColorHover, hoverAnimknob);

            if (knobActive || (ImGui::IsMouseDragging(0) && hoverSlider)) {
                float newVolume = (localMouse.x - sliderPos.x) / sliderSize.x;
                volume = std::clamp(int(newVolume * 130.0f), 0, 130);
                if (playercommand) {
                    playercommand->SetVolume(volume);
                    if (volume > 0 && isMuted)
                        playercommand->SetMute(false);
                    if (volume > 0)
                        g_lastVolumeBeforeMute = volume;
                }
                Cfg.UpdateVideoSettings([volume](AppSettings& s){
                    s.defaultVolume = volume;
                });
                Cfg.SaveVideo();
            }

            draw_list->AddCircleFilled(knobCenter, radius, knobColor);

            ImGui::PopID();

            ImGui::SameLine(); i = i + 0.4f;

            timePosX = controlPos.x + (spacing * i) + (sliderSize.x + 10.0f * scale);
            DrawTimeDisplay(time_show_ui, duration, videoSize, 
                ImVec2(timePosX, controlPos.y), iconSize.y);
        } else {
            ImGui::SameLine(); i = i + 0.4f;
            timePosX = controlPos.x + (spacing * i);
            DrawTimeDisplay(time_show_ui, duration, videoSize, 
                ImVec2(timePosX, controlPos.y), iconSize.y);
        }

        ImGui::EndGroup();

        ImGui::SameLine();
        i = i - 1.0f;

        static SettingsIconData settingsData;
        settingsData.opened = showSettings;
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * 16, controlPos.y));
        if (CSImGui::CustomIconButton("##setting", DrawSettingsIconAnimated, iconSize, &settingsData)) {
            Cfg.LoadAll();
            showSettings = !showSettings;
        }

        ImVec2 settingsIconPos = ImGui::GetItemRectMin();

        CSImGui::ToolTip("Settings", 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);

        RenderIOCHSidebar(runtime, videoPos, videoSize, showSettings, settingsIconPos);

        settingsData.hovered = ImGui::IsItemHovered();

        i = i + 11.5f;

        bool isFullscreen_video = runtime->state.display.isFullscreen;
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * 17, controlPos.y));
        static FullscreenIconData fsData;
        fsData.fullscreen = isFullscreen_video;
        if (CSImGui::CustomIconButton("##FullscreenToggle", DrawFullscreenIconAnimated, iconSize, &fsData)) {
            runtime->properties.Set<bool>("TriggerToggleFullscreen", true);
        }

        CSImGui::ToolTip(isFullscreen_video ? "Exit Fullscreen##Btnfs" : "Fullscreen##Btnfs", 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);

        static OptionIconData optdata;
        optdata.opened = SidarBarPopup.IsOpen();
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * 18, controlPos.y));
        if (CSImGui::CustomIconButton("##option", DrawOptionIconAnimated, iconSize, &optdata)) {
            if (SidarBarPopup.IsOpen()){
                SidarBarPopup.Close();
            }else{
                OpenSidarBarPopup(SidarBarPopup);
            }
            showOptionMenu = !showOptionMenu;
        }
        optdata.hovered = ImGui::IsItemHovered();

        CSImGui::ToolTip("Options", 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);

        if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()){
            if(showSettings) showSettings = !showSettings;
            else {
                if (playercommand) {
                    if(paused)
                        playercommand->Play();
                    else 
                        playercommand->Pause();
                }
            }
        }
    }

    items_action = (Volume_action || seek_bar_action);
    items_hover = (header_hoverd || volume_hover || seek_bar_hover);

    ImGui::EndChild();
    ImGui::PopStyleVar();
}