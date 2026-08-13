#include "gui_text.h"
#include <cmath>
#include <stdarg.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <imgui_internal.h>

void CSImGui::ModernTextEffect(const char* text, const TextEffectStyle& style) {
    if (!text || *text == '\0') return;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImGuiContext& g = *GImGui;
    ImFont* font = g.Font;
    float fontSize = g.FontSize * style.fontScale;
    ImDrawList* drawList = window->DrawList;

    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::ItemSize(textSize);
    if (!ImGui::ItemAdd(ImRect(pos, ImVec2(pos.x + textSize.x, pos.y + textSize.y)), 0)) return;

    if (style.effect == TextEffect::Gradient) {
        drawList->AddText(font, fontSize, pos, IM_COL32(255, 255, 255, 255), text);
        ImVec2 maxPos = ImVec2(pos.x + textSize.x, pos.y + textSize.y);
        drawList->AddRectFilledMultiColor(pos, maxPos, 
            style.colorTopLeft, style.colorTopLeft, 
            style.colorBottomRight, style.colorBottomRight);
        return;
    }

    if (style.effect == TextEffect::Outline) {
        float t = style.outlineThickness;
        drawList->AddText(font, fontSize, ImVec2(pos.x - t, pos.y), style.outlineColor, text);
        drawList->AddText(font, fontSize, ImVec2(pos.x + t, pos.y), style.outlineColor, text);
        drawList->AddText(font, fontSize, ImVec2(pos.x, pos.y - t), style.outlineColor, text);
        drawList->AddText(font, fontSize, ImVec2(pos.x, pos.y + t), style.outlineColor, text);
        
        drawList->AddText(font, fontSize, pos, style.colorTopLeft, text);
        return;
    }

    if (style.effect == TextEffect::Wave) {
        float time = (float)ImGui::GetTime() * style.waveSpeed;
        ImVec2 curPos = pos;

        const char* p = text;
        while (*p) {
            const char* p_next = p + 1;
            
            float charOffset = (p - text) * 0.5f;
            float offsetY = std::sin(time * style.waveFrequency + charOffset) * style.waveAmplitude;

            char buf[5] = {0};
            int bytes = p_next - p;
            memcpy(buf, p, bytes);

            drawList->AddText(font, fontSize, ImVec2(curPos.x, curPos.y + offsetY), style.colorTopLeft, buf);

            ImVec2 charSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
            curPos.x += charSize.x;
            p = p_next;
        }
        return;
    }

    if (style.effect == TextEffect::Typewriter) {
        size_t totalLen = strlen(text);
        size_t visibleLen = (size_t)(totalLen * ImClamp(style.progress, 0.0f, 1.0f));

        std::string visibleText(text, visibleLen);
        drawList->AddText(font, fontSize, pos, style.colorTopLeft, visibleText.c_str());
        return;
    }

    if (style.effect == TextEffect::Shimmer) {
        drawList->AddText(font, fontSize, pos, style.colorTopLeft, text);

        float time = (float)fmod(ImGui::GetTime() * style.speed, 2.0f) - 0.5f;
        float shimmerX = pos.x + (textSize.x * time);

        drawList->PushClipRect(pos, ImVec2(pos.x + textSize.x, pos.y + textSize.y), true);
        drawList->AddRectFilledMultiColor(
            ImVec2(shimmerX - 15.0f, pos.y), 
            ImVec2(shimmerX + 15.0f, pos.y + textSize.y),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 180),
            IM_COL32(255, 255, 255, 180), IM_COL32(255, 255, 255, 0)
        );
        drawList->PopClipRect();
        return;
    }

    if (style.effect == TextEffect::Scrolling) {
        float viewWidth = (style.scrollWidth > 0.0f) ? style.scrollWidth : ImGui::GetContentRegionAvail().x;
        ImRect viewRect(pos, ImVec2(pos.x + viewWidth, pos.y + textSize.y));

        ImGui::ItemSize(ImVec2(viewWidth, textSize.y));
        if (!ImGui::ItemAdd(viewRect, 0)) return;

        bool isHovered = ImGui::IsItemHovered();
        bool shouldScroll = (textSize.x > viewWidth) && (!style.scrollOnlyOnHover || isHovered);

        if (!shouldScroll) {
            drawList->PushClipRect(viewRect.Min, viewRect.Max, true);
            drawList->AddText(font, fontSize, pos, style.colorTopLeft, text);
            drawList->PopClipRect();
            return;
        }

        float totalWidth = textSize.x + style.scrollGap;
        float time = (float)ImGui::GetTime();
        float offset = std::fmod(time * style.scrollSpeed, totalWidth);

        drawList->PushClipRect(viewRect.Min, viewRect.Max, true);

        ImVec2 textPos1 = ImVec2(pos.x - offset, pos.y);
        drawList->AddText(font, fontSize, textPos1, style.colorTopLeft, text);

        ImVec2 textPos2 = ImVec2(pos.x - offset + totalWidth, pos.y);
        drawList->AddText(font, fontSize, textPos2, style.colorTopLeft, text);

        drawList->PopClipRect();
        return;
    }

    drawList->AddText(font, fontSize, pos, style.colorTopLeft, text);
}

void CSImGui::ModernTextEffectFmt(const TextEffectStyle& style, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ModernTextEffect(buf, style);
}

void CSImGui::TextGradient(const char* text, ImU32 colStart, ImU32 colEnd) {
    TextEffectStyle style;
    style.effect = TextEffect::Gradient;
    style.colorTopLeft = colStart;
    style.colorBottomRight = colEnd;
    ModernTextEffect(text, style);
}

void CSImGui::TextWave(const char* text, float speed) {
    TextEffectStyle style;
    style.effect = TextEffect::Wave;
    style.waveSpeed = speed;
    style.colorTopLeft = IM_COL32(255, 255, 255, 255);
    ModernTextEffect(text, style);
}

void CSImGui::TextScrolling(const char* text, float maxWidth, float speed, bool scrollOnlyOnHover) {
    TextEffectStyle style;
    style.effect = TextEffect::Scrolling;
    style.scrollWidth = maxWidth;
    style.scrollSpeed = speed;
    style.scrollOnlyOnHover = scrollOnlyOnHover;
    ModernTextEffect(text, style);
}