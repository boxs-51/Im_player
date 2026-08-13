#include "icon_system.h"
#include <cmath>
#include <imgui_internal.h>
#include "utils.h"

void DrawSettingsIconAnimated(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data) {
    SettingsIconData* d = (SettingsIconData*)user_data;
    if (!d) return;

    float dt = ImGui::GetIO().DeltaTime;
    float speed = 8.0f;

    d->hover_t += (d->hovered ? 1.0f : -1.0f) * dt * speed;
    d->open_t  += (d->opened  ? 1.0f : -1.0f) * dt * speed;

    d->hover_t = ImClamp(d->hover_t, 0.0f, 1.0f);
    d->open_t  = ImClamp(d->open_t,  0.0f, 1.0f);

    d->angle += d->open_t * dt * 2.5f;

    ImVec2 center = ImVec2((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
    float w = pMax.x - pMin.x;

    float radiusOuter = w * 0.35f;
    float radiusInner = radiusOuter * 0.5f;
    float toothLen    = w * 0.1f;
    int   teeth       = 8;

    float breathe = 1.0f + sinf(d->hover_t * IM_PI * 2.0f) * 0.04f;
    radiusOuter *= breathe;
    radiusInner *= breathe;

    drawList->AddCircle(center, radiusOuter, color, 24, w * 0.08f);

    for (int i = 0; i < teeth; i++) {
        float a = d->angle + i * (IM_PI * 2.0f / teeth);
        float c = cosf(a);
        float s = sinf(a);

        ImVec2 p1(center.x + c * (radiusOuter - w * 0.05f), center.y + s * (radiusOuter - w * 0.05f));
        ImVec2 p2(center.x + c * (radiusOuter + toothLen), center.y + s * (radiusOuter + toothLen));

        drawList->AddLine(p1, p2, color, w * 0.12f);
    }

    drawList->AddCircleFilled(center, radiusInner * 0.6f, color);
}

void DrawFullscreenIconAnimated(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data) {
    FullscreenIconData* data = (FullscreenIconData*)user_data;
    if (!data) return;

    float target = data->fullscreen ? 1.0f : 0.0f;
    data->t = ImLerp(data->t, target, 0.18f);
    float t = ImClamp(data->t, 0.0f, 1.0f);

    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;
    float lineLen = w * 0.25f;
    float thick = 2.0f;

    float inset = ImLerp(0.0f, w * 0.12f, t);

    ImVec2 aMin = ImVec2(pMin.x + inset, pMin.y + inset);
    ImVec2 aMax = ImVec2(pMax.x - inset, pMax.y - inset);

    ImU32 baseA = (color >> 24) & 0xFF;
    ImU32 alpha = (ImU32)(baseA * (0.85f + 0.15f * (1.0f - t)));
    ImU32 col = (color & 0x00FFFFFF) | (alpha << 24);

    drawList->AddLine(aMin, ImVec2(aMin.x + lineLen, aMin.y), col, thick);
    drawList->AddLine(aMin, ImVec2(aMin.x, aMin.y + lineLen), col, thick);

    drawList->AddLine(ImVec2(aMax.x, aMin.y), ImVec2(aMax.x - lineLen, aMin.y), col, thick);
    drawList->AddLine(ImVec2(aMax.x, aMin.y), ImVec2(aMax.x, aMin.y + lineLen), col, thick);

    drawList->AddLine(ImVec2(aMin.x, aMax.y), ImVec2(aMin.x + lineLen, aMax.y), col, thick);
    drawList->AddLine(ImVec2(aMin.x, aMax.y), ImVec2(aMin.x, aMax.y - lineLen), col, thick);

    drawList->AddLine(aMax, ImVec2(aMax.x - lineLen, aMax.y), col, thick);
    drawList->AddLine(aMax, ImVec2(aMax.x, aMax.y - lineLen), col, thick);
}

void DrawFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float w = pMax.x - pMin.x;
    float lineLen = w * 0.25f;
    float thick = 2.0f;

    drawList->AddLine(pMin, ImVec2(pMin.x + lineLen, pMin.y), color, thick);
    drawList->AddLine(pMin, ImVec2(pMin.x, pMin.y + lineLen), color, thick);

    drawList->AddLine(ImVec2(pMax.x, pMin.y), ImVec2(pMax.x - lineLen, pMin.y), color, thick);
    drawList->AddLine(ImVec2(pMax.x, pMin.y), ImVec2(pMax.x, pMin.y + lineLen), color, thick);

    drawList->AddLine(ImVec2(pMin.x, pMax.y), ImVec2(pMin.x + lineLen, pMax.y), color, thick);
    drawList->AddLine(ImVec2(pMin.x, pMax.y), ImVec2(pMin.x, pMax.y - lineLen), color, thick);

    drawList->AddLine(pMax, ImVec2(pMax.x - lineLen, pMax.y), color, thick);
    drawList->AddLine(pMax, ImVec2(pMax.x, pMax.y - lineLen), color, thick);
}

void DrawUnFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;
    float lineLen = w * 0.25f;
    float thick = 2.0f;

    ImVec2 cMin = ImVec2(pMin.x + w*0.1f, pMin.y + h*0.1f);
    ImVec2 cMax = ImVec2(pMax.x - w*0.1f, pMax.y - h*0.1f);

    drawList->AddLine(cMin, ImVec2(cMin.x + lineLen, cMin.y), color, thick);
    drawList->AddLine(cMin, ImVec2(cMin.x, cMin.y + lineLen), color, thick);

    drawList->AddLine(cMax, ImVec2(cMax.x - lineLen, cMax.y), color, thick);
    drawList->AddLine(cMax, ImVec2(cMax.x, cMax.y - lineLen), color, thick);
}

void DrawOptionIconAnimated(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data) {
    OptionIconData* d = (OptionIconData*)user_data;
    if (!d) return;

    float dt = ImGui::GetIO().DeltaTime;
    float speed = 4.0f;

    if (d->hovered) {
        if (d->hover_t < 1.0f) d->hover_t += dt * speed;
    } else {
        d->hover_t = 0.0f;
    }

    d->hover_t = ImClamp(d->hover_t, 0.0f, 1.0f);

    d->open_t += (d->opened ? 1.0f : -1.0f) * dt * 8.0f;
    d->open_t = ImClamp(d->open_t, 0.0f, 1.0f);

    float ht = d->hover_t;
    float ot = d->open_t;

    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;

    float baseR = w * 0.08f;
    float cx = pMin.x + w * 0.5f;

    float stretch = ImLerp(1.0f, 3.0f, ot);

    float y[3] = {
        pMin.y + h * 0.25f,
        pMin.y + h * 0.50f,
        pMin.y + h * 0.75f
    };

    float slideAmount = w * 0.12f;
    float phaseStep  = 0.4f;

    for (int i = 0; i < 3; i++) {
        float phase = i * phaseStep;
        float envelope = sinf(ht * IM_PI);
        float wave     = sinf(ht * IM_PI + phase);
        float slide = envelope * wave * slideAmount;

        if (d->opened) slide = 0.0f;

        drawList->AddEllipseFilled(
            ImVec2(cx + slide, y[i]),
            ImVec2(baseR * stretch, baseR),
            color
        );
    }
}

void DrawLoadingIconAnimated(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data) {
    LoadingIconData* d = (LoadingIconData*)user_data;
    if (!d) return;

    if (d->drawList) drawList = d->drawList;

    float dt = ImGui::GetIO().DeltaTime;
    d->angle += dt * d->speed;
    if (d->angle > IM_PI * 2.0f) d->angle -= IM_PI * 2.0f;

    ImVec2 finalMin = (d->size.x > 0) ? d->pos : pMin;
    ImVec2 finalMax = (d->size.x > 0) ? (d->pos + d->size) : pMax;

    ImVec2 center = ImVec2((finalMin.x + finalMax.x) * 0.5f, (finalMin.y + finalMax.y) * 0.5f);
    float radius = (finalMax.x - finalMin.x) * 0.4f;
    float thickness = (finalMax.x - finalMin.x) * 0.1f;

    ImU32 bgColor = (color & 0x00FFFFFF) | (0x33 << 24);
    drawList->AddCircle(center, radius, bgColor, 30, thickness);

    float arcLength = IM_PI * 0.5f + (sinf(d->angle) * 0.5f + 0.5f) * IM_PI;
    
    drawList->PathArcTo(center, radius, d->angle, d->angle + arcLength, 30);
    drawList->PathStroke(color, false, thickness);
}