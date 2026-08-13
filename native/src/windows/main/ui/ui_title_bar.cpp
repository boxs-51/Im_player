#include "ui_title_bar.h"

#include "windows/WindowRuntime.h"
#include "windows/WindowSnapshot.h"
#include "gui/gui.h"
#include "utils.h"

#include <string>


void RenderTitleBarWindowObject(WindowRuntime* runtime, const char* title, const char* subtitle, ImVec2 _winPos, ImVec2 _winSize) {
    
#ifdef CUSTOM_TITLEBAR
    const auto& state = runtime->GetSnapshot()->GetState();

    bool isFullscreen = state.display.isFullscreen;
    bool isMaximized  = state.display.isMaximized;
    bool isPinned     = state.display.isPinned;

    bool mouseDownMin     = state.input.mouseDownMin;
    bool mouseDownMax     = state.input.mouseDownMax;
    bool mouseDownRestore = state.input.mouseDownRestore;
    bool mouseDownClose   = state.input.mouseDownClose;
    bool mouseDownPin     = state.input.mouseDownPin;

    ImVec4 minColor   = state.display.minColor;
    ImVec4 maxColor   = state.display.maxColor;
    ImVec4 closeColor = state.display.closeColor;
    ImVec4 pinColor   = state.display.pinColor;

    if (!runtime->resource.sdlWindow || isFullscreen) return;

    ImVec2 titlePos = (_winPos);
    ImVec2 titleSize = (_winSize);
    float dt = ImGui::GetIO().DeltaTime;
    ImGui::SetNextWindowPos(titlePos);

    std::string title_bar = "##TitleBar_" + runtime->info.templateName + "_" + std::to_string(runtime->info.id);
    ImGui::BeginChild(title_bar.c_str(), titleSize);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::SetCursorScreenPos(titlePos);

    std::string title_bar_region = "##TitleBarRegion_" + runtime->info.templateName + "_" + std::to_string(runtime->info.id);
    ImGui::InvisibleButton(title_bar_region.c_str(), titleSize);

    // Background TitleBar
    ImU32 btnBgColor = IM_COL32(35, 35, 35, 255);
    dl->AddRectFilled(titlePos, titlePos + titleSize, btnBgColor);

    // =========================================================================
    // 1. RENDER TITLE & SUBTITLE (Hiển thị thường + Subtitle tương tác)
    // =========================================================================
    float textStartY = titlePos.y + (runtime->style.titleHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImVec2 textCursorPos(titlePos.x + 12.0f, textStartY);

    // Giới hạn vùng chứa văn bản chiếm khoảng 55% chiều rộng cửa sổ
    float maxTitleAreaWidth = titleSize.x * 0.55f; 

    // --- MAIN TITLE (Hiển thị tĩnh bình thường, cắt nếu quá dài) ---
    std::string truncatedMainTitle = TextUtils::TruncateTextByPixels(title, maxTitleAreaWidth);
    
    dl->AddText(
        textCursorPos,
        IM_COL32(255, 255, 255, 255),
        truncatedMainTitle.c_str()
    );

    // --- SUBTITLE (Cắt chuỗi & Hiệu ứng tương tác Hover/Click) ---
    if (subtitle && *subtitle != '\0') {
        ImVec2 mainTitleSize = ImGui::CalcTextSize(truncatedMainTitle.c_str());
        float subTitleStartX = textCursorPos.x + mainTitleSize.x + 10.0f;
        float remainingWidth = maxTitleAreaWidth - mainTitleSize.x - 10.0f;

        // Chỉ hiển thị Subtitle nếu còn đủ khoảng trống (> 30px)
        if (remainingWidth > 30.0f) {
            std::string truncatedSubTitle = TextUtils::TruncateTextByPixels(subtitle, remainingWidth);
            ImVec2 subTitlePos(subTitleStartX, textStartY);
            ImVec2 subTitleSize = ImGui::CalcTextSize(truncatedSubTitle.c_str());

            // Đặt Cursor để tạo Button tàng hình kiểm tra Hover/Click cho Subtitle
            ImGui::SetCursorScreenPos(subTitlePos);
            std::string sub_btn_id = "##SubTitleBtn_" + runtime->info.templateName + "_" + std::to_string(runtime->info.id);
            ImGui::InvisibleButton(sub_btn_id.c_str(), subTitleSize);

            bool subHovered = ImGui::IsItemHovered();
            bool subActive  = ImGui::IsItemActive();

            // Cấu hình hiệu ứng động theo trạng thái tương tác
            TextEffectStyle subStyle;
            subStyle.fontScale = 0.88f;

            if (subActive) {
                // Trạng thái Click: Nổi viền Outline màu xanh nhấn
                subStyle.effect = TextEffect::Outline;
                subStyle.colorTopLeft = IM_COL32(0, 200, 255, 255);
                subStyle.outlineColor = IM_COL32(0, 50, 100, 200);
                subStyle.outlineThickness = 1.0f;
            } 
            else if (subHovered) {
                // Trạng thái Hover: Hiệu ứng sóng Sóng/Wave nhịp nhàng
                subStyle.effect = TextEffect::Wave;
                subStyle.colorTopLeft = IM_COL32(255, 255, 255, 255);
                subStyle.waveSpeed = 4.0f;
                subStyle.waveAmplitude = 2.0f;
                subStyle.waveFrequency = 0.5f;
            } 
            else {
                // Trạng thái Bình thường: Màu xám nhẹ tĩnh
                subStyle.effect = TextEffect::Outline;
                subStyle.colorTopLeft = IM_COL32(160, 160, 160, 180);
                subStyle.outlineColor = IM_COL32(20, 20, 20, 120);
                subStyle.outlineThickness = 1.0f;
            }

            ImGui::SetCursorScreenPos(subTitlePos);
            CSImGui::ModernTextEffect(truncatedSubTitle.c_str(), subStyle);
        }
    }

    const ImVec2 btnSize(runtime->style.btnSize, runtime->style.titleHeight);
    const float pad = 8.0f;

    // =========================================================================
    // 2. CLOSE BUTTON ❌
    // =========================================================================
    ImVec2 closePos(titlePos.x + titleSize.x - btnSize.x, titlePos.y);
    ImGui::SetCursorScreenPos(closePos);

    std::string close_btn = "##CloseBtn_" + runtime->info.templateName + "_" + std::to_string(runtime->info.id);
    bool clickedClose = ImGui::InvisibleButton(close_btn.c_str(), btnSize);
    bool hoveredClose = ImGui::IsItemHovered();
    bool activeClose  = (hoveredClose && mouseDownClose);
    CSImGui::ToolTip("Close", 2.0f, ToolTipFlags_Animation);

    ImVec4 target_close = ImVec4(0.12f, 0.12f, 0.12f, 0.00f);
    if (hoveredClose) target_close = ImVec4(0.91f, 0.22f, 0.28f, 1.0f);
    if (activeClose)  target_close = ImVec4(0.72f, 0.11f, 0.18f, 1.0f);

    closeColor = ImLerp(closeColor, target_close, SMOOTH_LERP(12.0f, dt));

    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.display.closeColor = closeColor;
    }

    dl->AddRectFilled(closePos, closePos + btnSize, ToCol32(closeColor), 4.0f);
    {
        float iconPad   = 7.0f;   
        float lineThick = 2.0f;
        float lineScale = 1.0f;
        float linesize1 = 30.0f;
        float linesize2 = 30.0f;

        ImVec2 center = ImVec2(closePos.x + btnSize.x * 0.5f, closePos.y + btnSize.y * 0.5f);

        float angle1 = 45.0f;   
        float angle2 = -45.0f;  

        float halfLen1 = (linesize1 * 0.5f - iconPad) * lineScale;
        float halfLen2 = (linesize2 * 0.5f - iconPad) * lineScale;

        auto Deg2Rad = [](float deg){ return deg * 3.14159265f / 180.0f; };

        ImVec2 dir1 = ImVec2(cosf(Deg2Rad(angle1)) * halfLen1, sinf(Deg2Rad(angle1)) * halfLen1);
        ImVec2 dir2 = ImVec2(cosf(Deg2Rad(angle2)) * halfLen2, sinf(Deg2Rad(angle2)) * halfLen2);

        ImVec2 c1 = center - dir1; ImVec2 c2 = center + dir1;
        ImVec2 c3 = center - dir2; ImVec2 c4 = center + dir2;

        ImU32 color = IM_COL32(255,255,255,255);
        dl->AddLine(c1, c2, color, lineThick);
        dl->AddLine(c3, c4, color, lineThick);
    }

    // =========================================================================
    // 3. MAXIMIZE 🗖 / RESTORE 🗗 BUTTON
    // =========================================================================
    ImVec2 maxPos(closePos.x - btnSize.x, titlePos.y);
    ImGui::SetCursorScreenPos(maxPos);

    std::string max_restore_btn = "##MaxRestoreBtn_" + runtime->info.templateName + "_" + std::to_string(runtime->info.id);
    bool clickedMax = ImGui::InvisibleButton(max_restore_btn.c_str(), btnSize);
    bool hoveredMax = ImGui::IsItemHovered();
    bool activeMax  = (hoveredMax && (mouseDownMax || mouseDownRestore));

    CSImGui::ToolTip(isMaximized ? "Restore" : "Maximize", 2.0f, ToolTipFlags_Animation);

    ImVec4 targetMax = ImVec4(0.12f, 0.12f, 0.12f, 0.00f);
    if (hoveredMax) targetMax = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    if (activeMax)  targetMax = ImVec4(1.00f, 1.00f, 1.00f, 0.22f);

    maxColor = ImLerp(maxColor, targetMax, SMOOTH_LERP(12.0f, dt));

    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.display.maxColor = maxColor;
    }
        
    dl->AddRectFilled(maxPos, maxPos + btnSize, ToCol32(maxColor), 4.0f);
    {
        float iconPad      = 7.0f;    
        float iconBorder   = 0.0f;    
        float iconRounding = 1.0f;   
        float iconScaleY   = 0.8f;    
        float iconScaleX   = 0.8f;    
        if (!isMaximized) {
            ImVec2 iconPos = ImVec2(maxPos.x + iconPad, maxPos.y + iconPad);
            ImVec2 iconSize = ImVec2(
                (btnSize.x - 2 * iconPad) * iconScaleX,
                (btnSize.y - 2 * iconPad) * 1.0f
            );
            dl->AddRect(iconPos, iconPos + iconSize, IM_COL32(255,255,255,255), iconRounding, 0, iconBorder);
        } else {
            ImVec2 backPos  = ImVec2(maxPos.x + iconPad + 2, maxPos.y + iconPad + 2);
            ImVec2 frontPos = ImVec2(maxPos.x + iconPad,     maxPos.y + iconPad);
            ImVec2 iconSize = ImVec2(
                (btnSize.x - 2 * iconPad - 2) * iconScaleX,
                (btnSize.y - 2 * iconPad - 2) * (iconScaleY + 0.4f)
            );
            dl->AddRect(backPos,  backPos  + iconSize, IM_COL32(255,255,255,255), iconRounding, 0, iconBorder);
            dl->AddRect(frontPos, frontPos + iconSize, IM_COL32(255,255,255,255), iconRounding, 0, iconBorder);
        }
    }

    // =========================================================================
    // 4. MINIMIZE ➖ BUTTON
    // =========================================================================
    ImVec2 minPos(maxPos.x - btnSize.x, titlePos.y);
    ImGui::SetCursorScreenPos(minPos);
    
    std::string min_btn = "##MinBtn_" + runtime->info.templateName + "_" + std::to_string(runtime->info.id);
    bool clickedMin = ImGui::InvisibleButton(min_btn.c_str(), btnSize);
    bool hoveredMin = ImGui::IsItemHovered();
    bool activeMin  = (hoveredMin && mouseDownMin);

    CSImGui::ToolTip("Minimize", 2.0f, ToolTipFlags_Animation);

    ImVec4 target_min = ImVec4(0.12f, 0.12f, 0.12f, 0.00f);
    if (hoveredMin) target_min = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    if (activeMin)  target_min = ImVec4(1.00f, 1.00f, 1.00f, 0.22f);

    minColor = ImLerp(minColor, target_min, SMOOTH_LERP(12.0f, dt));
    
    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.display.minColor = minColor;
    }
    
    dl->AddRectFilled(minPos, minPos + btnSize, ToCol32(minColor), 4.0f);
    {
        float linesize = 30.f;
        ImVec2 lineStart = ImVec2(minPos.x + pad, minPos.y + btnSize.y / 2);
        ImVec2 lineEnd   = ImVec2(minPos.x + linesize - pad, minPos.y + btnSize.y / 2);
        dl->AddLine(lineStart, lineEnd, IM_COL32(255,255,255,255), 2.0f);
    }

    // =========================================================================
    // 5. PIN / ALWAYS ON TOP 📌 BUTTON
    // =========================================================================
    ImVec2 pinPos(minPos.x - btnSize.x, titlePos.y);
    ImGui::SetCursorScreenPos(pinPos);

    std::string pin_btn = "##PinBtn_" + runtime->info.templateName + "_" + std::to_string(runtime->info.id);
    bool clickedPin = ImGui::InvisibleButton(pin_btn.c_str(), btnSize);
    bool hoveredPin = ImGui::IsItemHovered();
    bool activePin  = (hoveredPin && mouseDownPin);

    CSImGui::ToolTip(isPinned ? "Unpin Window" : "Pin Always-On-Top", 2.0f, ToolTipFlags_Animation);

    ImVec4 targetPin = isPinned ? ImVec4(0.20f, 0.55f, 0.90f, 0.40f) : ImVec4(0.12f, 0.12f, 0.12f, 0.00f);
    if (hoveredPin) targetPin = isPinned ? ImVec4(0.25f, 0.60f, 1.00f, 0.60f) : ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    if (activePin)  targetPin = ImVec4(0.20f, 0.55f, 0.90f, 0.80f);

    pinColor = ImLerp(pinColor, targetPin, SMOOTH_LERP(12.0f, dt));

    {
        std::lock_guard<std::mutex> lock(runtime->stateMutex);
        runtime->state.display.pinColor = pinColor;
    }

    dl->AddRectFilled(pinPos, pinPos + btnSize, ToCol32(pinColor), 4.0f);

    {
        ImVec2 center = ImVec2(pinPos.x + btnSize.x * 0.5f, pinPos.y + btnSize.y * 0.5f);
        ImU32 pinIconColor = isPinned ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 180, 180, 255);

        dl->AddLine(center, ImVec2(center.x, center.y + 6.0f), pinIconColor, 1.5f);
        dl->AddCircleFilled(ImVec2(center.x, center.y - 3.0f), 3.5f, pinIconColor);
        dl->AddLine(ImVec2(center.x - 4.0f, center.y + 1.0f), ImVec2(center.x + 4.0f, center.y + 1.0f), pinIconColor, 2.0f);
    }

    ImGui::EndChild();
#endif
}