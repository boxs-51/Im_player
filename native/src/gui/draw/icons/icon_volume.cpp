#include "icon_volume.h"
#include <imgui_internal.h>

void DrawVolumeIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, void* user_data) {
    VolumeIconData* data = (VolumeIconData*)user_data;
    if (!data) return;

    int volume = data->volume;
    bool isMuted = data->isMuted || volume == 0;

    float waveTarget = isMuted ? 0.0f : 1.0f;
    float muteTarget = isMuted ? 1.0f : 0.0f;

    data->waveT = ImLerp(data->waveT, waveTarget, 0.15f);
    data->muteT = ImLerp(data->muteT, muteTarget, 0.2f);

    float waveT = ImClamp(data->waveT, 0.0f, 1.0f);
    float muteT = ImClamp(data->muteT, 0.0f, 1.0f);

    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;
    float centerY = pMin.y + h * 0.5f;

    float boxW = w * 0.25f;
    float boxH = h * 0.3f;

    ImVec2 speakerBoxMin(pMin.x, centerY - boxH * 0.5f);
    ImVec2 speakerBoxMax(pMin.x + boxW, centerY + boxH * 0.5f);

    drawList->AddRectFilled(speakerBoxMin, speakerBoxMax, color);

    ImVec2 pts[4] = {
        ImVec2(speakerBoxMax.x, speakerBoxMin.y),
        ImVec2(speakerBoxMax.x, speakerBoxMax.y),
        ImVec2(pMin.x + w * 0.5f, pMax.y - h * 0.15f),
        ImVec2(pMin.x + w * 0.5f, pMin.y + h * 0.15f),
    };
    drawList->AddConvexPolyFilled(pts, 4, color);

    if (muteT > 0.01f) {
        ImU32 baseA = (color >> 24) & 0xFF;
        ImU32 a = (ImU32)(baseA * muteT);
        ImU32 col = (color & 0x00FFFFFF) | (a << 24);

        float xSize = w * 0.15f * (0.7f + 0.3f * muteT);
        float xPos = pMin.x + w * 0.7f;

        drawList->AddLine(
            ImVec2(xPos - xSize, centerY - xSize),
            ImVec2(xPos + xSize, centerY + xSize),
            col, 2.0f);

        drawList->AddLine(
            ImVec2(xPos + xSize, centerY - xSize),
            ImVec2(xPos - xSize, centerY + xSize),
            col, 2.0f);
    }

    if (waveT > 0.01f) {
        int waves = (volume > 66) ? 3 : (volume > 33) ? 2 : 1;

        ImU32 baseA = (color >> 24) & 0xFF;
        ImU32 a = (ImU32)(baseA * waveT);
        ImU32 waveColor = (color & 0x00FFFFFF) | (a << 24);

        for (int j = 0; j < waves; j++) {
            float radius = ((w * 0.22f) + (j * w * 0.14f)) * (0.85f + 0.15f * waveT);
            drawList->PathArcTo(
                ImVec2(pMin.x + w * 0.35f, centerY),
                radius,
                -IM_PI * 0.3f,
                 IM_PI * 0.3f
            );
            drawList->PathStroke(waveColor, false, 2.0f);
        }
    }
}