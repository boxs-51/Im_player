#include "icon_media.h"
#include <imgui_internal.h>

void DrawPlayPauseIcon(ImDrawList* dl, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data) {
    PlayPauseData* data = (PlayPauseData*)user_data;
    if (!data) return;

    float target = data->paused ? 1.0f : 0.0f;
    data->t = ImLerp(data->t, target, 0.15f); 
    float t = ImClamp(data->t, 0.0f, 1.0f);

    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;
    
    float paddingX = w * 0.20f;
    float paddingY = h * 0.18f;
    float barW = w * 0.25f;
    float gap = w * 0.10f;

    ImVec2 a1 = ImVec2(pMin.x + paddingX, pMin.y + paddingY);
    ImVec2 a2 = ImVec2(pMin.x + paddingX, pMax.y - paddingY);
    ImVec2 a3 = ImVec2(pMax.x - paddingX, pMin.y + h * 0.5f);

    ImVec2 b1 = ImVec2(pMin.x + paddingX, pMin.y + paddingY);
    ImVec2 b2 = ImVec2(pMin.x + paddingX, pMax.y - paddingY);
    ImVec2 b3 = ImVec2(pMin.x + paddingX + barW, pMin.y + paddingY);
    ImVec2 b4 = ImVec2(pMin.x + paddingX + barW, pMax.y - paddingY);

    ImVec2 p1 = ImLerp(a1, b1, t);
    ImVec2 p2 = ImLerp(a2, b2, t);
    ImVec2 p3 = ImLerp(a3, b3, t);
    ImVec2 p4 = ImLerp(a3, b4, t);

    dl->AddQuadFilled(p1, p3, p4, p2, color);

    if (t > 0.01f) {
        float alphaFactor = ImClamp((t - 0.2f) / 0.8f, 0.0f, 1.0f);
        ImU32 baseColorAlpha = (color >> 24) & 0xFF;
        ImU32 newAlpha = (ImU32)(baseColorAlpha * alphaFactor);
        ImU32 color2 = (color & 0x00FFFFFF) | (newAlpha << 24);

        float currentGap = t * (barW + gap);
        ImVec2 bar2_min = ImVec2(p1.x + currentGap, pMin.y + paddingY);
        ImVec2 bar2_max = ImVec2(p3.x + currentGap, pMax.y - paddingY);

        if (newAlpha > 0) {
            dl->AddRectFilled(bar2_min, bar2_max, color2, 1.0f);
        }
    }
}

void DrawPlayIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    ImVec2 p1 = pMin;
    ImVec2 p2 = ImVec2(pMin.x, pMax.y);
    ImVec2 p3 = ImVec2(pMax.x, (pMin.y + pMax.y) * 0.5f);
    drawList->AddTriangleFilled(p1, p2, p3, color);
}

void DrawPauseIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float width = pMax.x - pMin.x;
    float barWidth = width * 0.35f;
    drawList->AddRectFilled(pMin, ImVec2(pMin.x + barWidth, pMax.y), color);
    drawList->AddRectFilled(ImVec2(pMax.x - barWidth, pMin.y), pMax, color);
}

void DrawPrevIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float width = pMax.x - pMin.x;
    float barWidth = width * 0.15f;
    drawList->AddRectFilled(pMin, ImVec2(pMin.x + barWidth, pMax.y), color);
    ImVec2 p1 = ImVec2(pMax.x, pMin.y);
    ImVec2 p2 = ImVec2(pMax.x, pMax.y);
    ImVec2 p3 = ImVec2(pMin.x + barWidth, (pMin.y + pMax.y) * 0.5f);
    drawList->AddTriangleFilled(p1, p2, p3, color);
}

void DrawNextIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float width = pMax.x - pMin.x;
    float barWidth = width * 0.15f;
    ImVec2 p1 = pMin;
    ImVec2 p2 = ImVec2(pMin.x, pMax.y);
    ImVec2 p3 = ImVec2(pMax.x - barWidth, (pMin.y + pMax.y) * 0.5f);
    drawList->AddTriangleFilled(p1, p2, p3, color);
    drawList->AddRectFilled(ImVec2(pMax.x - barWidth, pMin.y), pMax, color);
}