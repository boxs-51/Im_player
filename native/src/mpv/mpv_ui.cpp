#define STB_IMAGE_IMPLEMENTATION
#include "mpv/mpv_custom_ui.h"
#include "mpv/mpv_ui_settings.h"
#include "mpv/mpv_controller.h"
#include "mpv/mpv_settings.h"
#include "mpv/mpv_ui.h"
#include <mpv/mpv_data.h>

#include "windows/windows_borderless.h"

#include "popup/popup.h"

#include "utils.h"
#include "SDL.h"

#include <map>
#include <algorithm>

static DragResizeState& g_DragResizeState = GetDragResizeState();
static VideoInfo& g_videoInfo = GetVideoInfo();
static MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
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

void DrawTimeDisplay(double current, double duration, ImVec2 videoSize, ImVec2 pos, float parentHeight) {
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
void RenderPlayerControls(mpv_handle* mpv, ImVec2 videoPos, ImVec2 videoSize,
                          bool& isFullscreen_video,bool& show_ui_video)
 {


    if(!show_ui_video)
        showSettings = false;

    static float uiAlpha = 0.0f;
    UpdateHoverAnim(uiAlpha,show_ui_video,5.0f);
    // Nếu đã hoàn toàn ẩn thì bỏ qua render để tiết kiệm
    if (uiAlpha <= 0.01f)
        return; 
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

    int currentChapterIndex = -1;
    if (!g_videoInfo.g_chapters.empty()) {
        for (int i = 0; i < (int)g_videoInfo.g_chapters.size(); i++) {
            double start = g_videoInfo.g_chapters[i].time;
            double end   = (i + 1 < (int)g_videoInfo.g_chapters.size()) ? g_videoInfo.g_chapters[i+1].time : g_playbackStatus.duration;

            if (g_playbackStatus.playbackTime >= start && g_playbackStatus.playbackTime < end) {
                currentChapterIndex = i;
                break;
            }
        }
    }

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
    int PADDING_HEADER = 10;
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
    CusTomImGui::ShowTooltipDelayed(g_playbackStatus.mediaTitle.empty() ? "No Title" : g_playbackStatus.mediaTitle.c_str(), is_text_hovered , 3.0, "Header_Text_Part");
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

    if (duration > 0.0f) {
        ImGui::SetCursorPos(ImVec2(controlPosBar));
        ImGui::SetNextItemWidth(sliderWidth);

        // === Slider vô hình để giữ layout và nhận tương tác ===
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0)); // ẩn grab
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
        static bool wasDraggingSeekbar = false;
        static bool isDraggingSeekbar = false;
        static bool isHoveringSeek =false;
        static float time_show = 0.0f; // Dùng cho preview khi kéo seek
        if (duration > 0.0f) {
            // Cập nhật giá trị time_show khi không kéo
            if (!isDraggingSeekbar)
                time_show = playbackTime;

            ImVec2 seekSize(sliderWidth, 10.0f * scale);
            ImGui::InvisibleButton("##SeekBar", seekSize);
            wasDraggingSeekbar = isDraggingSeekbar;
            isDraggingSeekbar = ImGui::IsItemActive();
            isHoveringSeek = ImGui::IsItemHovered();
            
            seek_bar_action = isDraggingSeekbar;
            seek_bar_hover = isHoveringSeek;


            if(isDraggingSeekbar){
                ImVec2 mouse = ImGui::GetMousePos();
                float mouseX = std::clamp(mouse.x, ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().x);
                float percent = (mouseX - ImGui::GetItemRectMin().x) / (ImGui::GetItemRectMax().x - ImGui::GetItemRectMin().x);
                time_show = duration * percent;
            }
        }
        ImGui::PopStyleColor(2);

        // === Lấy toạ độ ===
        ImVec2 rectMin = ImGui::GetItemRectMin();
        ImVec2 rectMax = ImGui::GetItemRectMax();
        ImRect sliderRect(rectMin, rectMax);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        float sliderHeight = sliderRect.GetHeight();
        float sliderY = (sliderRect.Min.y + sliderRect.Max.y) * 0.5f;
        float progress = time_show / duration;
        float sliderStartX = sliderRect.Min.x;
        float sliderEndX = sliderRect.Max.x;
        float sliderWidthActual = sliderEndX - sliderStartX;
        float filledX = sliderStartX + sliderWidthActual * progress;

        // === Vẽ nền seekbar ===
        // === Hiệu ứng phóng to khi hover ===
        static float hoverAnim = 0.0f; // giá trị từ 0 -> 1
        UpdateHoverAnim(hoverAnim,isHoveringSeek,12.0f);

        // phóng to thêm 50% khi hover
        float baseBarHeight = 6.0f * scale;
        float barHeight = baseBarHeight * (1.0f + 0.5f * hoverAnim);
        float barY = sliderY - barHeight * 0.5f;

        draw_list->AddRectFilled(ImVec2(sliderStartX, barY), ImVec2(sliderEndX, barY + barHeight),
                                IM_COL32(60, 60, 60, 180), barHeight * 0.5f); // nền xám

        // === Vẽ phần đã cache(xám trắng)  ===
        float bufferProgress = g_playbackStatus.demuxer_cache_time / duration;
        bufferProgress = std::clamp(bufferProgress, 0.0f, 1.0f);
        float bufferedX = sliderStartX + sliderWidthActual * bufferProgress;

        draw_list->AddRectFilled(ImVec2(sliderStartX, barY), ImVec2(bufferedX, barY + barHeight),
                                IM_COL32(200, 200, 200, 150), barHeight * 0.5f);

        
        // === Vẽ phần đã xem (đỏ) ===
        draw_list->AddRectFilled(ImVec2(sliderStartX, barY), ImVec2(filledX, barY + barHeight),
                                IM_COL32(255, 60, 60, 220), barHeight * 0.5f);  
        // === Vẽ nút hình tròn ===
        float knobRadius = 8.0f * scale * (1.0f + 0.3f * hoverAnim);
        ImVec2 knobCenter = ImVec2(filledX, sliderY);
        draw_list->AddCircleFilled(knobCenter, knobRadius, IM_COL32(255, 255, 255, 255));
        draw_list->AddCircle(knobCenter, knobRadius, IM_COL32(0, 0, 0, 200)); // viền
        // === Vẽ markers cho chapters ===
        for (auto &c : g_videoInfo.g_chapters) {
            float chapterProgress = (float)(c.time / duration);
            float chapterX = sliderStartX + sliderWidthActual * chapterProgress;

            ImVec2 center(chapterX, barY - 6.0f * scale);
            float r = 5.0f * scale;
            draw_list->AddQuadFilled(
                ImVec2(center.x, center.y - r),
                ImVec2(center.x + r, center.y),
                ImVec2(center.x, center.y + r),
                ImVec2(center.x - r, center.y),
                IM_COL32(255, 255, 255, 200)
            );
            
            // Vẽ vạch nhỏ
            draw_list->AddLine(
                ImVec2(chapterX, barY),
                ImVec2(chapterX, barY + barHeight),
                IM_COL32(255, 200, 0, 200), // màu vàng cam
                2.0f
            );

            // (tuỳ chọn) Vẽ chấm nhỏ phía trên
            draw_list->AddCircleFilled(ImVec2(chapterX, barY - 4.0f * scale),
                                    3.0f * scale,
                                    IM_COL32(255, 200, 0, 220));

            draw_list->AddRectFilled(
                ImVec2(chapterX - 2.0f * scale, barY),
                ImVec2(chapterX + 2.0f * scale, barY + barHeight),
                IM_COL32(70, 70, 255, 200),
                1.0f
            );
        }
        if (currentChapterIndex >= 0) {
            double start = g_videoInfo.g_chapters[currentChapterIndex].time;
            double end   = (currentChapterIndex + 1 < (int)g_videoInfo.g_chapters.size()) ? g_videoInfo.g_chapters[currentChapterIndex+1].time : g_playbackStatus.duration;

            float startX = sliderStartX + sliderWidthActual * (start / g_playbackStatus.duration);
            float endX   = sliderStartX + sliderWidthActual * (end   / g_playbackStatus.duration);

            draw_list->AddRectFilled(
                ImVec2(startX, barY),
                ImVec2(endX, barY + barHeight),
                IM_COL32(100, 100, 255, 60),   // xanh mờ
                barHeight * 0.5f
            );
        }

        // === Hover hiện thời gian + (title nếu ở đúng chapter) ===
        ImVec2 mousePos = ImGui::GetMousePos();
        if (sliderRect.Contains(mousePos)) {
            float mouseX = std::clamp(mousePos.x, sliderStartX, sliderEndX);
            float percent = (mouseX - sliderStartX) / sliderWidthActual;
            float hoverTime = duration * percent;

            // Format thời gian
            char timeText[32];
            int totalSec = (int)hoverTime;
            int hours = totalSec / 3600;
            int min = (totalSec % 3600) / 60;
            int sec = totalSec % 60;

            if (hours > 0)
                snprintf(timeText, sizeof(timeText), "%d:%02d:%02d", hours, min, sec);
            else
                snprintf(timeText, sizeof(timeText), "%02d:%02d", min, sec);

            // Kiểm tra có hover vào chapter marker không
            std::string chapterTitle;
            bool onChapter = false;
            for (auto &c : g_videoInfo.g_chapters) {
                float chapterProgress = (float)(c.time / duration);
                float chapterX = sliderStartX + sliderWidthActual * chapterProgress;
                if (fabs(mouseX - chapterX) <= 5.0f * scale) { // trong 5px
                    chapterTitle = c.title;
                    hoverTime = c.time; // snap đúng mốc chapter
                    onChapter = true;
                    break;
                }
            }

            // Tooltip
            ImVec2 timeSize  = ImGui::CalcTextSize(timeText);
            ImVec2 titleSize = chapterTitle.empty() ? ImVec2(0,0) : ImGui::CalcTextSize(chapterTitle.c_str());
            float tooltipW = std::max(timeSize.x, titleSize.x) + 12.0f; // padding X
            float tooltipH = timeSize.y + (chapterTitle.empty() ? 0.0f : titleSize.y) + 8.0f * scale; // padding Y
            ImVec2 tooltipPos(mouseX - tooltipW * 0.5f, barY - tooltipH - 15.0f * scale);

            // Clamp X
            if (tooltipPos.x <  videoPos.x + 4.0f) 
                tooltipPos.x =  videoPos.x + 4.0f;
            if (tooltipPos.x + tooltipW >  videoPos.x + videoSize.x - 4.0f)
                tooltipPos.x =  videoPos.x + videoSize.x - tooltipW - 4.0f;
            // Clamp Y
            if (tooltipPos.y <  videoPos.y + 4.0f) 
                tooltipPos.y =  videoPos.y + 4.0f;
            if (tooltipPos.y + tooltipH > videoPos.y + videoSize.y  - 4.0f)
                tooltipPos.y = videoPos.y  + videoSize.y  - tooltipH - 4.0f;

            draw_list->AddRectFilled(
                ImVec2(tooltipPos.x - 6, tooltipPos.y - 4),
                ImVec2(tooltipPos.x + tooltipW + 6, tooltipPos.y + tooltipH + 4),
                IM_COL32(0, 0, 0, 100), 4.0f
            );

            float yCursor = tooltipPos.y + 2;
            if (!chapterTitle.empty()) {
                draw_list->AddText(
                    ImVec2(tooltipPos.x + (tooltipW - titleSize.x) * 0.5f, yCursor),
                    IM_COL32(255, 230, 120, 255), chapterTitle.c_str()
                );
                yCursor += titleSize.y;
            }
            draw_list->AddText(
                ImVec2(tooltipPos.x + (tooltipW - timeSize.x) * 0.5f, yCursor),
                IM_COL32(255, 255, 255, 255), timeText
            );

            // Line marker
            
            draw_list->AddLine(ImVec2(mouseX, barY - 4), ImVec2(mouseX, barY + barHeight + 4),
                            IM_COL32(255, 255, 255, 100));


            // Khi vừa thả chuột ra khỏi seekbar → thực hiện tua
            if (wasDraggingSeekbar  && !isDraggingSeekbar) {
                double target = (float)totalSec;

                // dùng seek clamped theo delta
                float delta = (float)(target - playbackTime);
                mpv_command_seek_clamped(mpv, delta, playbackTime, duration);
                if(!onChapter){
                    mpv_command_seek_abs(mpv, target ,duration);
                }else{
                    mpv_command_seek_abs(mpv, hoverTime ,duration); 
                }

            }
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    if(!onlyShowSeekBar){
        // === CONTROL BUTTONS ===
        float i = 0.0f;
        if(!(g_playbackStatus.g_PlayingIndex == 0 && g_playbackStatus.g_playlist_count > 0)){
            ImGui::SetCursorPos(ImVec2(controlPos.x, controlPos.y)); i =  i + 1.0f ;
            if (CustomIconButton("##prev", DrawPrevIcon, iconSize)) {
                mpv_command_prev_video(mpv);
            }
            CusTomImGui::ShowTooltipDelayed("Previous Video", ImGui::IsItemHovered(), 3.0 ,"Prev_Button");
        }
        // Nút PLAY/PAUSE
        static  PlayPauseData playData;
        playData.paused = !paused;
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * i, controlPos.y)); i = i + 1.0f;
        if (CustomIconButton("##toggle", DrawPlayPauseIcon, iconSize ,&playData)) {
            if (paused) mpv_command_play(mpv);
            else        mpv_command_pause(mpv);
            

        }

        CusTomImGui::ShowTooltipDelayed(paused ? "Play" : "Pause", ImGui::IsItemHovered( ), 3.0 ,"PlayPause_Button");
        // Nút NEXT
        if(!(g_playbackStatus.g_PlayingIndex == (int)g_playbackStatus.g_playlist.size() - 1) && g_playbackStatus.g_playlist_count >= 2){
            ImGui::SetCursorPos(ImVec2(controlPos.x +  spacing * i , controlPos.y)); i = i + 2.0f;
            if (CustomIconButton("##next", DrawNextIcon, iconSize)) {
                mpv_command_next_video(mpv);
            }

            CusTomImGui::ShowTooltipDelayed("Next Video", ImGui::IsItemHovered(), 3.0, "Next_Button");
        }

        ImGui::BeginGroup();
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * i, controlPos.y)); 
        
        static VolumeIconData volData;
        volData.volume = volume;   // 0..100
        volData.isMuted = isMuted; // true/false


        CustomIconButton(
            "##volume",
            DrawVolumeIcon,
            iconSize,
            &volData
        );

        bool clicked = ImGui::IsItemClicked();
        bool hoverIcon = ImGui::IsItemHovered();

        CusTomImGui::ShowTooltipDelayed(isMuted ? "Unmute" : "Mute", hoverIcon , 3.0 ,"Volume_Button");

        bool active = ImGui::IsItemActive();

        if (clicked) {
            if (isMuted || volume == 0) {
                // Unmute → khôi phục lại âm lượng
                mpv_command_set_mute(mpv, false);
                mpv_command_set_volume(mpv, g_lastVolumeBeforeMute);
            } else {
                // Mute → ghi nhớ âm lượng rồi set 0
                g_lastVolumeBeforeMute = volume;
                mpv_command_set_volume(mpv, 0);
                mpv_command_set_mute(mpv, true);
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
                mpv_command_set_volume(mpv, volume);
                if (volume > 0 && isMuted)
                    mpv_command_set_mute(mpv, false);
                if (volume > 0)
                    g_lastVolumeBeforeMute = volume;
                v_Settings.defaultVolume = volume;
                SaveSettings_Video();
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
                mpv_command_set_volume(mpv, volume);
                if (volume > 0 && isMuted)
                    mpv_command_set_mute(mpv, false);
                if (volume > 0)
                    g_lastVolumeBeforeMute = volume;
                v_Settings.defaultVolume = volume;
                SaveSettings_Video();
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
        if (CustomIconButton("##setting", DrawSettingsIconAnimated, iconSize , &settingsData)) {
            LoadSettings_Video();
            showSettings = !showSettings;
        }

        ImVec2 iconPos = ImGui::GetItemRectMin();
        ImVec2 iconSize = ImGui::GetItemRectSize(); // Lấy kích thước nút

        CusTomImGui::ShowTooltipDelayed("Settings", ImGui::IsItemHovered(), 3.0 ,"Settings_Button");

        RenderIOCHSidebar(mpv ,videoPos, videoSize, showSettings, show_ui_video ,iconPos);

        settingsData.hovered = ImGui::IsItemHovered();


        i = i + 11.5f;
        // --- BUTTON FULLSCREEN ---
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * 17, controlPos.y));
        static FullscreenIconData fsData;
        fsData.fullscreen = isFullscreen_video ;
        if (CustomIconButton("##FullscreenToggle", DrawFullscreenIconAnimated, iconSize, &fsData)) {

            g_DragResizeState.ToggleFullscreen = true;
        }

        CusTomImGui::ShowTooltipDelayed(isFullscreen_video ? "Exit Fullscreen" : "Fullscreen", ImGui::IsItemHovered(), 3.0 ,"Fullscreen_Button");

        // --- BUTTON OPTION ---
        static OptionIconData optdata;
        optdata.opened  = SidarBarPopup.IsOpen();
        ImGui::SetCursorPos(ImVec2(controlPos.x + spacing * 18, controlPos.y));
        if (CustomIconButton("##option", DrawOptionIconAnimated, iconSize ,&optdata)) {
            if (SidarBarPopup.IsOpen()){
                SidarBarPopup.Close();
            }else{
                OpenSidarBarPopup(SidarBarPopup);
            }
            showOptionMenu = !showOptionMenu;
        }
        optdata.hovered = ImGui::IsItemHovered();

        CusTomImGui::ShowTooltipDelayed("Options", ImGui::IsItemHovered(), 3.0 ,"Option_Button");
    
        if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()){
            if(showSettings) showSettings = !showSettings;
            else {
                if(paused)
                    mpv_command_play(mpv);
                else 
                    mpv_command_pause(mpv);
            }
        }
    }

    items_action = (Volume_action || seek_bar_action);
    items_hover = (header_hoverd || volume_hover || seek_bar_hover);

    ImGui::EndChild ();
    ImGui::PopStyleVar();

}

void RenderIdleBackground(std::string imagePath, ImVec2 videopos, ImVec2 videoSize) {
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
        
        const ImVec2 p_min = videopos;
        const ImVec2 p_max = ImVec2(videopos.x + videoSize.x, videopos.y + videoSize.y);

        draw_list->AddImage(cachedImTexID, p_min, p_max);
    }
}
void CleanupIcons(){
    
}
void RenderLoading(ImVec2 VideoPos, ImVec2 VideoSize) {
    static LoadingIconData centralLoading;

    // --- LOGIC SCALE & CLAMP ---
    // 1. Tính toán kích thước lý tưởng (ví dụ: 10% chiều rộng video)
    float idealSize = VideoSize.x * 0.10f; 
    
    // 2. Giới hạn Min/Max để icon không bị quá bé hoặc quá to
    float minSize = 35.0f;
    float maxSize = 110.0f;
    float finalSize = ImClamp(idealSize, minSize, maxSize);

    // 3. Cập nhật vị trí trung tâm dựa trên size mới
    centralLoading.pos = ImVec2(
        VideoPos.x + (VideoSize.x * 0.5f) - (finalSize * 0.5f), 
        VideoPos.y + (VideoSize.y * 0.5f) - (finalSize * 0.5f)
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

void RenderSeekingOverlay(ImVec2 VideoPos, ImVec2 VideoSize ,SeekingData& data)  {
    float dt = ImGui::GetIO().DeltaTime;

    // 1. Alpha: Hiện nhanh (10.0f), ẩn chậm hơn (3.0f) để tạo cảm giác mượt
    float targetAlpha = data.g_isSeeking ? 1.0f : 0.0f;
    float lerpSpeed = data.g_isSeeking ? 10.0f : 3.0f;
    data.alpha = ImLerp(data.alpha, targetAlpha, ImMin(dt * lerpSpeed, 1.0f));

    // 2. Pulse: Giảm dần về 0. (Ví dụ: khi người dùng bấm phím mũi tên, bạn set pulse = 1.0f bên ngoài)
    data.pulse = ImLerp(data.pulse, 0.0f, ImMin(dt * 6.0f, 1.0f));

    // 3. Timer: Update liên tục nếu icon còn hiển thị
    if (data.alpha > 0.001f) {
        data.timer += dt * 2.5f; // Tốc độ sóng chạy
        if (data.timer > 1.0f) data.timer -= 1.0f; // Tránh mất frame
        
        data.pos  = VideoPos;
        data.size = VideoSize;

        // Gọi đúng tên hàm đã định nghĩa
        DrawSeekingIconAnimated(
            ImGui::GetWindowDrawList(), 
            ImVec2(0,0), ImVec2(0,0), 
            IM_COL32(255, 255, 255, 255), 
            &data
        );
        
    }
}



