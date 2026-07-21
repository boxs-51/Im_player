#include "mpv_ui_settings.h"
#include "mpv_ui.h"
#include <mpv_data.h>
#include <mpv_basic_formats.h>

#include "gui/gui.h"
#include "popup/popup.h"
#include "settings_manager.h"
#include "mpv/session/MPVSession.h"
#include "utils.h"
#include "windows/WindowManager.h"
#include "WindowRuntime.h"
#include "SDL.h"
#include "imgui.h"
#include <map>
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
static bool items_action=  false;
static bool items_hover = false;
static bool was_ui_video = false;



void DrawTimeDisplay(double current, double duration, ImVec2& videoSize, ImVec2& pos, float parentHeight) {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // 1. Tính toán tỉ lệ scale dựa trên chiều rộng video (Reference: 1920px)
    float baseFontSizeMultiplier = 2.0f;
    float dynamicScale = videoSize.x /  1920.0f  * baseFontSizeMultiplier;
    dynamicScale = std::max(dynamicScale, 0.5f); // Giới hạn nhỏ nhất

    // 2. Định dạng chuỗi thời gian
    int curH = (int)(current / 3600), curM = (int)(current / 60) % 60, curS = (int)current % 60;
    int durH = (int)(duration / 3600), durM = (int)(duration / 60) % 60, durS = (int)duration % 60;

    char timeStr[64];
    if (durH > 0) {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d / %02d:%02d:%02d", curH, curM, curS, durH, durM, durS);
    } else {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d / %02d:%02d", curM, curS, durM, durS);
    }

    // 3. Sử dụng FontScale mới trong ImGui 1.92
    // Lưu lại scale cũ
    float oldFontScale = ImGui::GetFont()->Scale; 
    
    // Áp dụng scale mới cho Font hiện tại
    ImGui::GetFont()->Scale = dynamicScale;
    ImGui::PushFont(ImGui::GetFont()); 

    // 4. Tính toán kích thước Text thực tế sau khi scale
    ImVec2 textSize = ImGui::CalcTextSize(timeStr);
    
    // Căn giữa trục Y (Công thức: Vị trí Y + (Chiều cao vùng chứa - Chiều cao chữ) / 2)
    float centeredY = pos.y + (parentHeight - textSize.y) * 0.5f;

    // 5. Render
    ImGui::SetCursorPos(ImVec2(pos.x, centeredY));
    ImGui::TextUnformatted(timeStr);

    // 6. Khôi phục trạng thái
    ImGui::PopFont();
    ImGui::GetFont()->Scale = oldFontScale;
}
//================================================================================================
void RenderPlayerControls(WindowRuntime* runtime, ImVec2& _pos, ImVec2& _size, bool& isFullscreen_video, bool& show_ui_video)
 {
    if(!show_ui_video)
        showSettings = false;


    static float uiAlpha = 0.0f;

    UpdateHoverAnim(uiAlpha,show_ui_video,5.0f);
    was_ui_video = true;
    // Nếu đã hoàn toàn ẩn thì bỏ qua render để tiết kiệm
    if (uiAlpha <= 0.01f){
        was_ui_video = false;
        return; 
    }

    auto& Cfg = ConfigManager::Instance();
    
    ImVec2 videoPos = (_pos);
    ImVec2 videoSize = (_size);
    MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
    VideoInfo& g_videoInfo = GetVideoInfo();
    bool endfile = g_playbackStatus.isCoreIdle;
    bool paused = g_playbackStatus.isPaused;
    float playbackTime = (float)g_playbackStatus.playbackTime;
    float time_show_ui = (float)g_playbackStatus.timePos;
    float duration = (float)g_playbackStatus.duration;
    int volume = (int)g_playbackStatus.volume;
    bool isMuted = g_playbackStatus.isMuted;
    

    float timePosX = 0.0f;
    float scale = std::clamp(videoSize.x / 1280.0f, 0.1f, 2.0f);
    float spacing = videoSize.x * 0.05;
    ImVec2 iconSize(35 * scale, 35 * scale);
    const float minWidthForControls = 750.0f ;
    const float minWHegthForControls = 450.0f ;
    bool onlyShowSeekBar = (videoSize.x <= minWidthForControls || videoSize.y  <= minWHegthForControls );
    // === CONTROL WINDOW (bao gồm cả SEEKBAR) ===

    ImVec2 Pos = ImVec2(videoPos.x,videoPos.y);
    ImVec2 Possize = ImVec2(videoSize.x , videoSize.y );

    float sliderWidth = videoSize.x * 0.95 ;
    ImVec2 controlPosBar ((videoSize.x - sliderWidth) * 0.5f, videoSize.y* 0.85f);
    ImVec2 controlPos(videoSize.x * 0.05f, videoSize.y * 0.90f);

    if (onlyShowSeekBar) {
        sliderWidth = videoSize.x; // full chiều ngang
        controlPosBar= ImVec2(0.0f, videoSize.y - (videoSize.y * 0.025f)); // đặt ở dưới video
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
    // === SEEK BAR ===

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0, 0, 0, 0));   // nền trong suốt
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1, 1, 1, 0.1f)); // hover nhẹ
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(1, 1, 1, 0.2f)); // click sáng hơn

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 1.0f*scale)); // mỏng
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

    float value = playbackTime;
    float v_min = 0.0f;
    float v_max = duration;

    SliderFlags flags = SliderFlags_Default | SliderFlags_EnableSmoothPreview | SliderFlags_Animation;

    ImGui::SetCursorPos(ImVec2(controlPosBar));
    
    bool changed = CSImGui::ModernSliderFloatEx(
        "##SeekBar",
        &value,
        v_min,
        v_max,
        6.0f * scale,
        8.0f * scale,
        "%.2f",
        sliderWidth,
        flags,

        // =========================
        // RENDER CALLBACK
        // =========================
        [&](Phase phase, Slot slot, SliderState* state, SliderRenderData* rd, ImDrawList* dl)
        {
            if(phase == Phase::Init){
                // =========================
                // BUFFER
                // =========================
                rd->buffer_t = g_playbackStatus.demuxer_cache_time / duration;

                // =========================
                // MARKERS (chapters)
                // =========================
                int currentChapterIndex = -1;
                double start_time= 0.0f;
                double end_time = 0.0f;
                if (!g_videoInfo.g_chapters.empty()) {
                    for (int i = 0; i < (int)g_videoInfo.g_chapters.size(); i++) {
                        start_time = g_videoInfo.g_chapters[i].time;
                        end_time   = (i + 1 < (int)g_videoInfo.g_chapters.size()) ? g_videoInfo.g_chapters[i+1].time : g_playbackStatus.duration;
                        if (g_playbackStatus.playbackTime >= start_time && g_playbackStatus.playbackTime < end_time) {
                            currentChapterIndex = i;
                            break;
                        }
                    }
                }
                if (currentChapterIndex >= 0 && duration > 0.0)
                {

                    float start_t = (float)(start_time / duration);
                    float end_t   = (float)(end_time   / duration);

                    // clamp để đảm bảo an toàn
                    rd->chapter_range_start = ImClamp(start_t, 0.0f, 1.0f);
                    rd->chapter_range_end   = ImClamp(end_t,   0.0f, 1.0f);
                }
                rd->markers.clear();
                rd->markers.reserve(g_videoInfo.g_chapters.size());
                for (auto& c : g_videoInfo.g_chapters)
                {
                    rd->markers.push_back(c.time / duration);
                }

                // =========================
                // COLORS
                // =========================
                rd->col_track          = ImVec4(0.235f, 0.235f, 0.235f, 0.706f); // (60, 60, 60, 180)
                rd->col_buffer         = ImVec4(0.784f, 0.784f, 0.784f, 0.588f); // (200, 200, 200, 150)
                rd->col_fill           = ImVec4(1.000f, 0.235f, 0.235f, 0.863f); // (255, 60, 60, 220)
                rd->col_grab           = ImVec4(1.000f, 1.000f, 1.000f, 1.000f); // (255, 255, 255, 255)
                rd->col_marker         = ImVec4(1.000f, 0.784f, 0.000f, 0.784f); // (255, 200, 0, 200)
                rd->col_grab_border    = ImVec4(0.000f, 0.000f, 0.000f, 0.784f); // (0, 0, 0, 200)
                rd->col_chapter_range  = ImVec4(0.392f, 0.392f, 1.000f, 0.235f); // (100, 100, 255, 60)
                rd->col_border         = ImVec4(0.300f, 0.300f, 0.300f, 1.000f); // Màu xám đậm cho viền
                rd->col_grab_shadow    = ImVec4(0.000f, 0.000f, 0.000f, 0.350f); // Đổ bóng nhẹ
                if(rd->active){
                    rd->col_fill           = ImVec4(1.00f, 0.10f, 0.10f, 1.00f); // Đỏ đậm rực khi đang kéo
                    rd->col_grab           = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
                }else if(rd->grab_hovered){
                    rd->col_fill           = ImVec4(1.00f, 0.30f, 0.30f, 0.90f);
                    rd->col_grab           = ImVec4(0.90f, 0.90f, 0.90f, 1.00f); // Hơi xám nhẹ khi hover grab
                }else if(rd->bar_hovered){
                    rd->col_track          = ImVec4(0.30f, 0.30f, 0.30f, 0.80f); // Nền sáng lên một chút
                }
                rd->height_on_hover = rd->height * 0.5;
                float target_alpha = (rd->hovered || rd->active) ? 1.0f : 0.0f;
                float speed = 12.0f; // Ví dụ tốc độ là 0.3 giây
                static float smooth_grab_alpha = 0.0f;
                smooth_grab_alpha = ImLerp(smooth_grab_alpha, target_alpha, SMOOTH_LERP(speed, state->dt));
                
                rd->col_grab.w        *= smooth_grab_alpha;
                rd->col_grab_border.w *= smooth_grab_alpha;
                rd->col_grab_shadow.w *= smooth_grab_alpha;
                if (!rd->hovered && !rd->active) {
                    rd->grab_radius = 0.0f; // Scale dần về 0 cùng lúc với fade alpha
                }
            }
            

            // =========================
            // OPTIONAL
            // =========================
            // rd->draw_grab = false; // nếu muốn ẩn knob
        },

        // =========================
        // TOOLTIP CALLBACK
        // =========================
        [&](Phase phase, Slot slot, TooltipData* td, ImDrawList* dl)
        {
            if (phase == Phase::Init)
            {
                char timeText[32];
                int sec = (int)td->item.seek_value;

                int h = sec / 3600;
                int m = (sec % 3600) / 60;
                int s = sec % 60;

                if (td->item.seek_value >= 0 && td->item.seek_value <= duration)
                {
                    if (h > 0)
                        td->config.text =  Format("%d:%02d:%02d",h, m, s);
                        //sprintf(timeText, "%d:%02d:%02d", h, m, s);
                    else
                         td->config.text =  Format("%02d:%02d", m, s);
                        //sprintf(timeText, "%02d:%02d", m, s);

                        
                    //strcpy(td->text, timeText);
                }
                if (!g_videoInfo.g_chapters.empty()) {
                    td->config.show_title = false; // Reset mặc định

                    for (size_t i = 0; i < g_videoInfo.g_chapters.size(); ++i) {
                        double startTime = g_videoInfo.g_chapters[i].time;
                        
                        // Xác định endTime: nếu là chapter cuối thì lấy thời lượng tổng, 
                        // nếu không thì lấy thời gian bắt đầu của chapter sau.
                        double endTime = (i + 1 < g_videoInfo.g_chapters.size()) 
                                        ? g_videoInfo.g_chapters[i + 1].time 
                                        : g_playbackStatus.duration; 

                        if (td->item.seek_value >= startTime && td->item.seek_value < endTime) {
                            td->config.show_title = true;
                            // Sử dụng strncpy hoặc snprintf để an toàn hơn strcpy
                            td->config.title = g_videoInfo.g_chapters[i].title.c_str();
                            //snprintf(td->config.title, sizeof(td->config.title), "%s", g_videoInfo.g_chapters[i].title.c_str());
                            break; 
                        }
                    }
                }
                td->config.max_width = 240.0f;
                td->config.max_height = 180.0f;
                td->config.align = ImGuiTooltip::Align_Center;
            }
        },

        // =========================
        // SEEK CALLBACK
        // =========================

        [&](const SliderSeekRequest* req)
        {
    
            SliderSeekResult res{};

            if ((req->from_drag || req->from_click) && !req->is_final)
            {
                // ===== PREVIEW =====
                res.accept = false; // ❗ không update *v
            }
            else
            {
                if(req->is_hovered){
                    // ===== COMMIT ===== //                    
                    if (runtime && runtime->mpvSession && runtime->mpvSession->GetCommander()) 
                        runtime->mpvSession->GetCommander()->Seek(req->new_value, duration);
                    res.value = req->new_value;
                    res.accept = true;
                }
            }

            return res;
        },

        nullptr // nav
    );
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    if(!onlyShowSeekBar){
        // === CONTROL BUTTONS ===
        float i = 0.0f;
        if(!(g_playbackStatus.g_PlayingIndex == 0 && g_playbackStatus.g_playlist_count > 0)){
            ImGui::SetCursorPos(ImVec2(controlPos.x, controlPos.y)); i =  i + 1.0f ;
            if (CSImGui::CustomIconButton("##prev", DrawPrevIcon, iconSize)) {
                if (runtime && runtime->mpvSession && runtime->mpvSession->GetCommander()) 
                    runtime->mpvSession->GetCommander()->PlaylistPrev();
            }
            //CSImGui::ShowTooltipDelayed("Previous Video", ImGui::IsItemHovered(), 3.0 ,"Prev_Button");
            CSImGui::ToolTip("Previous Video" ,3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);
        }
        // Nút PLAY/PAUSE
        static  PlayPauseData playData;
        playData.paused = !paused;
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * i, controlPos.y)); i = i + 1.0f;
        if (CSImGui::CustomIconButton("##toggle", DrawPlayPauseIcon, iconSize ,&playData)) {
            if (runtime && runtime->mpvSession && runtime->mpvSession->GetCommander()) {
                if (paused) runtime->mpvSession->GetCommander()->Play();
                else        runtime->mpvSession->GetCommander()->Pause();
            }

        }

        //CSImGui::ShowTooltipDelayed(paused ? "Play" : "Pause", ImGui::IsItemHovered( ), 3.0 ,"PlayPause_Button");
        CSImGui::ToolTip(paused ? "Play##Btntoggle" : "Pause##Btntoggle", 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);
        // Nút NEXT
        if(!(g_playbackStatus.g_PlayingIndex == (int)g_playbackStatus.g_playlist.size() - 1) && g_playbackStatus.g_playlist_count >= 2){
            ImGui::SetCursorPos(ImVec2(controlPos.x +  spacing * i , controlPos.y)); i = i + 2.0f;
            if (CSImGui::CustomIconButton("##next", DrawNextIcon, iconSize)) {
                if (runtime && runtime->mpvSession && runtime->mpvSession->GetCommander()) 
                    runtime->mpvSession->GetCommander()->PlaylistNext();
            }
            CSImGui::ToolTip("Next Video", 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);
            //CSImGui::ShowTooltipDelayed("Next Video", ImGui::IsItemHovered(), 3.0, "Next_Button");
        }

        ImGui::BeginGroup();
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * i, controlPos.y)); 
        
        static VolumeIconData volData;
        volData.volume = volume;   // 0..100
        volData.isMuted = isMuted; // true/false


        CSImGui::CustomIconButton(
            "##volume",
            DrawVolumeIcon,
            iconSize,
            &volData
        );

        bool clicked = ImGui::IsItemClicked();
        bool hoverIcon = ImGui::IsItemHovered();

        //CSImGui::ShowTooltipDelayed(isMuted ? "Unmute" : "Mute", hoverIcon , 3.0 ,"Volume_Button");
        CSImGui::ToolTip(isMuted ? "Unmute##BtnVol" : "Mute##BtnVol" , 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);

        bool active = ImGui::IsItemActive();

        if (clicked) {
            if (runtime && runtime->mpvSession && runtime->mpvSession->GetCommander()) {
                auto* commander = runtime->mpvSession->GetCommander();
                if (isMuted || volume == 0) {
                    // Unmute → khôi phục lại âm lượng
                    commander->SetMute(false);
                    commander->SetVolume(g_lastVolumeBeforeMute);
                } else {
                    // Mute → ghi nhớ âm lượng rồi set 0
                    g_lastVolumeBeforeMute = volume;
                    commander->SetVolume(0);
                    commander->SetMute(true);
                }
            }
        }
        // === Hiệu ứng xuất hiện / biến mất (fade + trượt ngang) ===
        bool showSlider = SDL_GetTicks64() < volumeSliderVisibleUntil;

        static float easedAnim = 0.0f; // giá trị từ 0 -> 1
        UpdateHoverAnim(easedAnim,showSlider,10.0f);

        // === VOLUME SLIDER (ẩn/hiện linh hoạt) ===
        ImVec2 baseSize(100.0f * scale, iconSize.y * 0.5f);
        ImVec2 sliderSize(baseSize.x * easedAnim, baseSize.y); i = i + 0.85; //4.85
        // Trượt ngang từ icon → vị trí cuối
        float slideOffset = ImLerp(-40.0f, 0.0f, easedAnim);
        ImVec2 sliderPos(controlPos.x + spacing * i + slideOffset,
                        controlPos.y + (iconSize.y - sliderSize.y) * 0.5f);
        // Kiểm tra xem chuột đang ở vùng slider "ảo"
        ImVec2 mouseScreen = ImGui::GetIO().MousePos;
        ImVec2 localMouse  = mouseScreen - ImGui::GetWindowPos(); // convert sang local

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
        // Cập nhật thời gian hiển thị nếu đang hover icon hoặc slider
        if (hoverIcon || hoverSlider || knobHovered || knobActive || isActive_Slider)
            volumeSliderVisibleUntil = SDL_GetTicks64() + 1000;  // 1 giây

        if (showSlider) {
            ImGui::SameLine();
            ImGui::SetCursorPos(sliderPos);  // đặt đúng vị trí
            ImGui::SetNextItemWidth(sliderSize.x);

            ImGui::PushID("VolumeSlider");
            ImGui::InvisibleButton("##volume_slider_invisible", sliderSize);

            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0)); // ẩn grab
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
            isHovered_Slider = ImGui::IsItemHovered();
            isActive_Slider = ImGui::IsItemActive();

            static float hoverAnim = 0.0f;// giá trị từ 0 -> 1
            static float hoverAnimknob = 0.0f;

            UpdateHoverAnim(hoverAnim,(isHovered_Slider || knobHovered || isActive_Slider),12.0f);

            if (isActive_Slider) {
                float newVolume = (localMouse.x - sliderPos.x) / sliderSize.x;
                volume = std::clamp(int(newVolume * 130.0f), 0, 130);
                if (runtime && runtime->mpvSession && runtime->mpvSession->GetCommander()) {
                    auto* commander = runtime->mpvSession->GetCommander();
                    commander->SetVolume(volume);
                    if (volume > 0 && isMuted)
                        commander->SetMute(false);
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

            // Vẽ nền thanh âm lượng
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

            ImU32 bgBase   = IM_COL32(80, 80, 80, 180);
            ImU32 bgHover  = IM_COL32(120, 120, 120, 220);
            ImU32 fillBase = IM_COL32(80, 160, 255, 255);
            ImU32 fillHover= IM_COL32(120, 200, 255, 255);

            ImU32 bgColor   = LerpColor(bgBase, bgHover, hoverAnim);
            ImU32 fillColor = LerpColor(fillBase, fillHover, hoverAnim);

            float barHalf = sliderSize.y * (0.25f + hoverAnim * 0.1f);
            float filledWidth = sliderSize.x * (volume / 130.0f);
            float centerY = sliderPos.y + sliderSize.y * 0.5f;
            
            ImVec2 barStart(sliderPos.x + winPos.x, centerY - barHalf + winPos.y);
            ImVec2 barEnd  (sliderPos.x + sliderSize.x + winPos.x, centerY + barHalf + winPos.y);
            ImVec2 filledEnd(sliderPos.x + filledWidth + winPos.x, centerY + barHalf + winPos.y);

            draw_list->AddRectFilled(barStart, barEnd, bgColor, sliderSize.y * 0.3f);
            draw_list->AddRectFilled(barStart, filledEnd, fillColor, sliderSize.y * 0.3f);

            // Vẽ knob
            float radius = (scale * 15.0f) * (1 + (hoverAnimknob * 0.1f));

            ImVec2 knobCenter(sliderPos.x + filledWidth + winPos.x, centerY + winPos.y);
            ImVec2 knobSize   = ImVec2(radius * 2, radius * 2);
            ImVec2 knobTopLeft = ImVec2(knobCenter.x - radius - winPos.x, knobCenter.y - radius - winPos.y);

            ImGui::SetCursorPos(knobTopLeft);
            ImGui::InvisibleButton("##volume_knob_drag", knobSize);

            knobActive = ImGui::IsItemActive();
            knobHovered = ImGui::IsItemHovered();
            Volume_action = knobActive || isActive_Slider;
            volume_hover = knobHovered || isHovered_Slider || knobActive;
            UpdateHoverAnim(hoverAnimknob,(knobHovered || isActive_Slider || knobActive),12.0f);

            ImU32 knobColorBase  = IM_COL32(255, 100, 100, 255);
            ImU32 knobColorHover = IM_COL32(255, 170, 170, 255);

            ImU32 knobColor = LerpColor(knobColorBase, knobColorHover, hoverAnimknob);

            if (knobActive || (ImGui::IsMouseDragging(0) && hoverSlider)) {
                float newVolume = (localMouse.x - sliderPos.x) / sliderSize.x;
                volume = std::clamp(int(newVolume * 130.0f), 0, 130);
                if (runtime && runtime->mpvSession && runtime->mpvSession->GetCommander()) {
                    auto* commander = runtime->mpvSession->GetCommander();
                    commander->SetVolume(volume);
                    if (volume > 0 && isMuted)
                        commander->SetMute(false);
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

            // Text thời gian
            ImGui::SameLine(); i = i + 0.4f;

            timePosX = controlPos.x + (spacing * i) + (sliderSize.x + 10.0f * scale);
            DrawTimeDisplay( time_show_ui, duration, videoSize, 
                ImVec2(timePosX, controlPos.y), iconSize.y);
        } else {
            ImGui::SameLine(); i = i + 0.4f;
            timePosX = controlPos.x + (spacing * i);
            DrawTimeDisplay( time_show_ui, duration, videoSize, 
                ImVec2(timePosX, controlPos.y), iconSize.y);
        }

        ImGui::EndGroup();


        // === FULLSCREEN BUTTON ===
        ImGui::SameLine();
    
        // Cập nhật vị trí và icon
        i = i - 1.0f;
        // --- BUTTON SETTINGS ---
        static SettingsIconData settingsData;

        settingsData.opened = showSettings;
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * 16, controlPos.y));
        // Thay thế ImageButton bằng CustomIconButton + DrawSettingsIcon
        if (CSImGui::CustomIconButton("##setting", DrawSettingsIconAnimated, iconSize , &settingsData)) {
            Cfg.LoadAll();
            showSettings = !showSettings;
        }

        ImVec2 iconPos = ImGui::GetItemRectMin();
        ImVec2 iconSize = ImGui::GetItemRectSize(); // Lấy kích thước nút

       // CSImGui::ShowTooltipDelayed("Settings", ImGui::IsItemHovered(), 3.0 ,"Settings_Button");
        CSImGui::ToolTip("Settings" , 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);

        RenderIOCHSidebar(runtime, videoPos, videoSize, showSettings, show_ui_video, iconPos);

        settingsData.hovered = ImGui::IsItemHovered();


        i = i + 11.5f;
        // --- BUTTON FULLSCREEN ---
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * 17, controlPos.y));
        static FullscreenIconData fsData;
        fsData.fullscreen = isFullscreen_video ;
        if (CSImGui::CustomIconButton("##FullscreenToggle", DrawFullscreenIconAnimated, iconSize, &fsData)) {

            //g_DragResizeState.ToggleFullscreen = true;
        }

        //CSImGui::ShowTooltipDelayed(isFullscreen_video ? "Exit Fullscreen" : "Fullscreen", ImGui::IsItemHovered(), 3.0 ,"Fullscreen_Button");
        CSImGui::ToolTip(isFullscreen_video ? "Exit Fullscreen##Btnfs" : "Fullscreen##Btnfs" , 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);
        // --- BUTTON OPTION ---
        static OptionIconData optdata;
        optdata.opened  = SidarBarPopup.IsOpen();
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * 18, controlPos.y));
        if (CSImGui::CustomIconButton("##option", DrawOptionIconAnimated, iconSize ,&optdata)) {
            if (SidarBarPopup.IsOpen()){
                SidarBarPopup.Close();
            }else{
                OpenSidarBarPopup(SidarBarPopup);
            }
            showOptionMenu = !showOptionMenu;
        }
        optdata.hovered = ImGui::IsItemHovered();

        //CSImGui::ShowTooltipDelayed("Options", ImGui::IsItemHovered(), 3.0 ,"Option_Button");
        CSImGui::ToolTip("Options" , 3.0f, ToolTipFlags_Animation | ToolTipFlags_ClampWindow);
        // --- BUTTON OPTION ---
        if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()){
            if(showSettings) showSettings = !showSettings;
            else { // Click vào vùng trống của video
                if (runtime && runtime->mpvSession && runtime->mpvSession->GetCommander()) {
                    if(paused)
                        runtime->mpvSession->GetCommander()->Play();
                    else 
                        runtime->mpvSession->GetCommander()->Pause();
                }
            }
        }
    }

    items_action = (Volume_action || seek_bar_action);
    items_hover = (header_hoverd || volume_hover || seek_bar_hover);

    ImGui::EndChild ();
    ImGui::PopStyleVar();

}

void RenderIdleBackground(std::string& imagePath, ImVec2& _pos, ImVec2& _size) {
    // 1. Dùng static để cache kết quả cuối cùng
    static ImTextureID cachedImTexID = (ImTextureID)0; 
    static std::string cachedPath = "";

    // 2. Nếu đường dẫn thay đổi, gọi GetIcon để lấy ID mới
    if (!imagePath.empty() && imagePath != cachedPath) {
        GLuint tex = GetIcon(imagePath);
        
        if (tex != 0) {
            // Ép kiểu từ GLuint sang ImTextureID (void*)
            cachedImTexID = (ImTextureID)(intptr_t)tex;
            cachedPath = imagePath;
        }
    }

    // 3. Nếu có texture hợp lệ thì vẽ
    if (cachedImTexID != (ImTextureID)0) {
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        
        const ImVec2 p_min = _pos;
        const ImVec2 p_max = _pos + _size;

        draw_list->AddImage(cachedImTexID, p_min, p_max);
    }
}
void CleanupIcons(){
    
}
void RenderLoading(ImVec2& _pos, ImVec2& _size) {

    static LoadingIconData centralLoading;
    // --- LOGIC SCALE & CLAMP ---
    // 1. Tính toán kích thước lý tưởng (ví dụ: 10% chiều rộng video)
    float idealSize = _size.x * 0.10f; 
    
    // 2. Giới hạn Min/Max để icon không bị quá bé hoặc quá to
    float minSize = 35.0f;
    float maxSize = 110.0f;
    float finalSize = ImClamp(idealSize, minSize, maxSize);

    // 3. Cập nhật vị trí trung tâm dựa trên size mới
    ImVec2 Pos = _pos;
    ImVec2 Size = _size;

    centralLoading.pos = ImVec2(
        Pos.x + (Size.x * 0.5f) - (finalSize * 0.5f), 
        Pos.y + (Size.y * 0.5f) - (finalSize * 0.5f)
    );
    centralLoading.size = ImVec2(finalSize, finalSize);

    // Vẽ với DrawList của cửa sổ hiện tại
    DrawLoadingIconAnimated(
        ImGui::GetWindowDrawList(), 
        ImVec2(0,0), ImVec2(0,0),
        IM_COL32(255, 255, 255, 255), 
        &centralLoading
    );

}

void RenderSeekingOverlay(ImVec2& _pos, ImVec2& _size) {
    // Thêm 'static' để giữ trạng thái của timer, alpha, pulse... qua từng frame
    static SeekingData data; 
    MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
    data.g_isSeeking = g_playbackStatus.isSeeking;
    data.currentTime = g_playbackStatus.timePos;

    float dt = ImGui::GetIO().DeltaTime;

    // 1. Trigger Pulse khi bắt đầu Seek
    if (data.g_isSeeking && !data.lastSeekingState) {
        data.pulse = 1.0f; // Bắt đầu hiệu ứng pulse
    }
    data.lastSeekingState = data.g_isSeeking;

    // 2. Logic cập nhật Forward
    if (data.g_isSeeking && data.currentTime != data.lastTime) {
        data.forward = (data.currentTime > data.lastTime);
    }
    data.lastTime = data.currentTime;

    // 3. Alpha: Hiện nhanh (10.0f), ẩn chậm hơn (3.0f) để tạo cảm giác mượt
    float targetAlpha = data.g_isSeeking ? 1.0f : 0.0f;
    float lerpSpeed = data.g_isSeeking ? 10.0f : 3.0f;
    data.alpha = ImLerp(data.alpha, targetAlpha, ImMin(dt * lerpSpeed, 1.0f));

    // 4. Pulse: Giảm dần về 0.
    data.pulse = ImLerp(data.pulse, 0.0f, ImMin(dt * 6.0f, 1.0f));

    // 5. Timer và Render: Cập nhật và vẽ trực tiếp nếu alpha đủ lớn
    if (data.alpha > 0.001f) {
        data.timer += dt * 2.5f; 
        if (data.timer > 1.0f) data.timer -= 1.0f; 
        
        data.pos  = _pos;
        data.size = _size;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 color = IM_COL32(255, 255, 255, 255);
        
        ImVec2 center;
        float fullW = data.size.x;
        float fullH = data.size.y;

        // Ưu tiên tọa độ video (không cần fallback vì data.size đã được set)
        if (data.forward) {
            center = ImVec2(data.pos.x + fullW * 0.75f, data.pos.y + fullH * 0.5f);
        } else {
            center = ImVec2(data.pos.x + fullW * 0.25f, data.pos.y + fullH * 0.5f);
        }

        float dir = data.forward ? 1.0f : -1.0f;
        
        // Áp dụng hiệu ứng Pulse vào kích thước (phóng to khi pulse > 0)
        float scale = 1.0f + (data.pulse * 0.2f); // Phóng to tối đa thêm 20%
        float triW = fullW * 0.05f * scale; 
        float triH = fullH * 0.20f * scale;
        float spacing = triW * 1.0f;

        float speed = 1.5f;
        float t = fmodf(data.timer * speed, 1.0f);

        // Chuẩn bị màu sắc
        ImVec4 colVec = ImGui::ColorConvertU32ToFloat4(color);
        ImVec4 brightCol = ImVec4(
            ImMin(colVec.x + 0.3f, 1.0f), 
            ImMin(colVec.y + 0.3f, 1.0f), 
            ImMin(colVec.z + 0.3f, 1.0f), 
            colVec.w * data.alpha
        );
        float baseAlpha = brightCol.w;

        ImU32 bgGlowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(brightCol.x, brightCol.y, brightCol.z, baseAlpha * 0.15f));
        ImU32 transparent = ImGui::ColorConvertFloat4ToU32(ImVec4(brightCol.x, brightCol.y, brightCol.z, 0.0f));

        // Vẽ Background Gradient
        if (data.forward) {
            drawList->AddRectFilledMultiColor(
                ImVec2(data.pos.x + fullW * 0.5f, data.pos.y), 
                ImVec2(data.pos.x + fullW, data.pos.y + fullH),
                transparent, bgGlowCol, bgGlowCol, transparent
            );
        } else {
            drawList->AddRectFilledMultiColor(
                ImVec2(data.pos.x, data.pos.y), 
                ImVec2(data.pos.x + fullW * 0.5f, data.pos.y + fullH),
                bgGlowCol, transparent, transparent, bgGlowCol
            );
        }

        int glowLayers = 6; 
        float maxGlowRadius = triH * 0.7f;

        // Vẽ hiệu ứng Glow (sáng tỏa tròn)
        for (int i = 0; i < 3; i++) {
            float offset = ((float)i + t) * spacing;
            float x = center.x + (offset - (spacing * 1.5f)) * dir;
            ImVec2 glowPos = ImVec2(x + (dir > 0 ? triW * 0.4f : -triW * 0.4f), center.y);

            for (int layer = 1; layer <= glowLayers; layer++) {
                float fraction = (float)layer / (float)glowLayers;
                float r = maxGlowRadius * fraction;
                float lAlpha = (1.0f - fraction) * 0.3f * baseAlpha; 
                
                if (lAlpha <= 0.0f) continue;
                
                ImU32 gCol = ImGui::ColorConvertFloat4ToU32(ImVec4(colVec.x, colVec.y, colVec.z, lAlpha));
                drawList->AddCircleFilled(glowPos, r, gCol, 16);
            }
        }

        // Vẽ 3 khối tam giác mũi tên
        for (int i = 0; i < 3; i++) {
            float offset = ((float)i + t) * spacing;
            float x = center.x + (offset - (spacing * 1.5f)) * dir;

            float progress = (float)(i + 1) / 3.0f;
            float triangleAlpha = baseAlpha * progress * (1.0f - t * 0.2f);
            
            ImU32 finalCol = ImGui::ColorConvertFloat4ToU32(ImVec4(colVec.x, colVec.y, colVec.z, triangleAlpha));

            ImVec2 p1, p2, p3;
            float tipX = (dir > 0) ? x + triW : x - triW;
            
            p1 = ImVec2(x, center.y - triH * 0.5f);
            p2 = ImVec2(x, center.y + triH * 0.5f);
            p3 = ImVec2(tipX, center.y);

            drawList->AddTriangleFilled(p1, p2, p3, finalCol);
        }
        // --- KẾT THÚC LOGIC VẼ ---
    }
}

void RenderGhostStatusOverlay(ImVec2& vPos, ImVec2& vSize, bool isPaused) {
    static PlayPauseOverlay s;
    float dt = ImGui::GetIO().DeltaTime;

    if (!s.initialized) {
        s.last_paused = isPaused;
        s.initialized = true;
        return;
    }

    // 1. Phát hiện thay đổi trạng thái
    if (isPaused != s.last_paused) {
        s.last_paused = isPaused;
        s.alpha = 1.0f;
        s.scale = 0.6f; 
    }

    // 2. Nội suy Alpha & Scale
    if (s.alpha > 0.0f) {
        s.alpha -= dt * 1.8f; // Tốc độ biến mất vừa phải
        s.scale = ImLerp(s.scale, 1.4f, dt * 5.0f); 
    }

    if (s.alpha > 0.001f) {
        // Sử dụng WindowDrawList để icon nằm đúng trong không gian video
        ImDrawList* dl = ImGui::GetWindowDrawList(); 
        
        ImVec2 center = ImVec2(vPos.x + vSize.x * 0.5f, vPos.y + vSize.y * 0.5f);
        float baseSize = (vSize.y * 0.08f) * s.scale; // Kích thước cơ bản

        // --- THIẾT LẬP MÀU SẮC ---
        ImVec4 iconColVec = ImVec4(1.0f, 1.0f, 1.0f, s.alpha * 0.9f);
        ImU32 iconCol = ImGui::ColorConvertFloat4ToU32(iconColVec);
        
        // Màu Glow (vòng tròn mờ phía sau)
        ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, s.alpha * 0.4f));

        // 3. VẼ VÒNG TRÒN GLOW (Nền bên dưới)
        // Tạo một vòng tròn đen mờ giúp icon trắng nổi bật hơn
        dl->AddCircleFilled(center, baseSize * 1.8f, glowCol, 36);

        // 4. VẼ ICON CHI TIẾT
        if (isPaused) {
            // Tinh chỉnh Pause: Rộng hơn, thấp hơn (Dày và chắc chắn)
            float barWidth = baseSize * 0.45f;  // Tăng độ rộng vạch
            float barHeight = baseSize * 1.1f;  // Giảm chiều cao tương đối
            float gap = baseSize * 0.25f;      // Khoảng cách giữa 2 vạch

            // Vạch trái
            dl->AddRectFilled(
                ImVec2(center.x - barWidth - gap, center.y - barHeight),
                ImVec2(center.x - gap, center.y + barHeight),
                iconCol, 5.0f); // Bo góc một chút cho hiện đại
            
            // Vạch phải
            dl->AddRectFilled(
                ImVec2(center.x + gap, center.y - barHeight),
                ImVec2(center.x + barWidth + gap, center.y + barHeight),
                iconCol, 5.0f);
        } 
        else {
            // Tinh chỉnh Play: Tam giác đều và mập hơn
            float pSize = baseSize * 1.2f;
            ImVec2 p1 = center + ImVec2(-pSize * 0.6f, -pSize * 0.9f);
            ImVec2 p2 = center + ImVec2(-pSize * 0.6f,  pSize * 0.9f);
            ImVec2 p3 = center + ImVec2( pSize * 1.0f,  0.0f);
            
            // Vẽ đổ bóng nhẹ cho tam giác
            dl->AddTriangleFilled(p1 + ImVec2(2,2), p2 + ImVec2(2,2), p3 + ImVec2(2,2), glowCol);
            dl->AddTriangleFilled(p1, p2, p3, iconCol);
        }
    }
}
