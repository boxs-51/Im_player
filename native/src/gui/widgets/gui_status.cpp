#include "gui_status.h"

#include "../core/gui_theme.h"

#include <cstdarg>
#include <cstdio>

namespace CSImGui
{

    namespace
    {

        constexpr float kBadgeRounding = 4.0f;

        constexpr float kBadgePaddingX = 8.0f;
        constexpr float kBadgePaddingY = 3.0f;

        constexpr float kBadgeIndicatorRadius = 3.0f;

        ImU32 ToColor32(const ImVec4 &color)
        {
            return ImGui::ColorConvertFloat4ToU32(color);
        }

        ImVec4 Darken(
            const ImVec4 &color,
            float factor)
        {
            return ImVec4(
                color.x * factor,
                color.y * factor,
                color.z * factor,
                color.w);
        }

        ImVec4 WithAlpha(
            const ImVec4 &color,
            float alpha)
        {
            return ImVec4(
                color.x,
                color.y,
                color.z,
                alpha);
        }

        ImVec4 GetSemanticColor(
            StatusType type)
        {
            switch (type)
            {
            case StatusType::Success:
            case StatusType::Active:
                return ImVec4(0.20f, 0.85f, 0.40f, 1.0f);

            case StatusType::Info:
                return ImVec4(0.25f, 0.65f, 1.00f, 1.0f);

            case StatusType::Warning:
                return ImVec4(1.00f, 0.65f, 0.20f, 1.0f);

            case StatusType::Error:
                return ImVec4(1.00f, 0.25f, 0.30f, 1.0f);

            case StatusType::Disabled:
            case StatusType::Inactive:
                return ImVec4(0.55f,0.58f,0.62f,1.0f);

            case StatusType::Processing:
                return ImVec4(0.35f,0.75f,1.00f,1.0f);

            case StatusType::Loading:
                return ImVec4(0.70f,0.55f,1.00f,1.0f);

            case StatusType::None:
            default:
                return CSImGui::GetColors(Col_Text);
            }
        }

    } 

    StatusColors GetStatusColors(StatusType type) {
        const ImVec4 semantic = GetSemanticColor(type);
            
        StatusColors result{};
        // Text
        result.text = semantic;

        // Background
        result.background = WithAlpha(Darken(semantic, 0.45f), 0.28f);
            
        // Border
        result.border = WithAlpha(semantic, 0.60f);

        // Indicator
        result.indicator = semantic;
            
        return result;
    }

    void TextStatus(const char *text, StatusType type){
        if (!text) return;
            
        const StatusColors colors = GetStatusColors(type);
            
        ImGui::PushStyleColor(ImGuiCol_Text, colors.text);
            
        ImGui::TextUnformatted(text);

        ImGui::PopStyleColor();
    }

    // ============================================================
    // TextStatusFmt
    // ============================================================

    void TextStatusFmt(StatusType type, const char *fmt, ...) {
        if (!fmt) return;
            
        char buffer[1024];

        va_list args;
        va_start(args, fmt);

#if defined(_MSC_VER)
        vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
#else
        vsnprintf(buffer, sizeof(buffer), fmt, args);
#endif
        va_end(args);

        TextStatus(buffer, type);
    }

    void Badge(const char *text, StatusType type, BadgeFlags flags) {
        if (!text) return;
            
        const StatusColors colors = GetStatusColors(type);

        const bool compact = flags & BadgeFlags_Compact;
        const bool noRounding = flags & BadgeFlags_NoRounding;

        const ImVec2 textSize = ImGui::CalcTextSize(text);

        const float paddingX = compact ? 4.0f : kBadgePaddingX;
        const float paddingY = compact ? 1.0f : kBadgePaddingY;
        float indicatorSpace = 0.0f;

        if (flags & BadgeFlags_Indicator)
            indicatorSpace = kBadgeIndicatorRadius * 2.0f + 6.0f;

        const ImVec2 size((textSize.x + paddingX * 2.0f + indicatorSpace), (textSize.y + paddingY * 2.0f));
  
        const ImVec2 pos = ImGui::GetCursorScreenPos();
            
        ImDrawList *drawList = ImGui::GetWindowDrawList();
            
        if (flags & BadgeFlags_Filled)
        {
            drawList->AddRectFilled(pos,
                ImVec2((pos.x + size.x), (pos.y + size.y)),
                ToColor32(colors.background),
                noRounding ? 0.0f : kBadgeRounding
            );        
        }

        if (flags & BadgeFlags_Border)
        {
            drawList->AddRect(pos,
                ImVec2(pos.x + size.x, pos.y + size.y),
                ToColor32(colors.border),
                noRounding ? 0.0f : kBadgeRounding,
                0,
                1.0f);
        }

        float textX = pos.x + paddingX;

        if (flags & BadgeFlags_Indicator)
        {
            const float centerX = pos.x + paddingX + kBadgeIndicatorRadius;
            const float centerY = pos.y + size.y * 0.5f;
                
            drawList->AddCircleFilled(
                ImVec2(centerX, centerY),
                kBadgeIndicatorRadius,
                ToColor32(colors.indicator));

            textX += kBadgeIndicatorRadius * 2.0f + 6.0f;      
        }

        drawList->AddText(
            ImVec2(textX, pos.y + paddingY),
            ToColor32(colors.text),
            text);

        ImGui::Dummy(size);
    }

    void BadgeFmt(
        const char *fmt,
        StatusType type,
        BadgeFlags flags, ...)
    {
        if (!fmt)
            return;

        char buffer[1024];

        va_list args;
        va_start(args, fmt);

#if defined(_MSC_VER)
        vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
#else
        vsnprintf(buffer, sizeof(buffer), fmt, args);
#endif

        va_end(args);

        Badge(buffer, type, flags);
    }

    // ============================================================
    // Convenience API
    // ============================================================

    void SuccessBadge(const char *text) {
        Badge(text, StatusType::Success);
    }

    void InfoBadge(const char *text) {
        Badge(text, StatusType::Info);  
    }

    void WarningBadge(const char *text) {
        Badge(text, StatusType::Warning);    
    }

    void ErrorBadge(const char *text) {
        Badge(text, StatusType::Error);   
    }
    void DisabledBadge(const char *text){
        Badge(text, StatusType::Disabled);    
    }

    void ActiveBadge(const char *text){
        Badge(text, StatusType::Active);     
    }

    void InactiveBadge(const char *text){
        Badge(text, StatusType::Inactive);   
    }

}