#include "gui_draw_helpers.h"

void CSImGui::DrawCardWithHole(ImDrawList* dl, const ImVec2& cardMin,
    const ImVec2& cardMax, const ImVec2& holeMin, const ImVec2& holeMax,
    ImU32 fillCol, ImU32 borderCol, const CardHoleStyle& style) {
    const float r = style.rounding;

    // FILL
    dl->PushClipRect(ImVec2(holeMax.x, cardMin.y), cardMax, true);
    dl->AddRectFilled(cardMin, cardMax, fillCol, r, ImDrawFlags_RoundCornersRight);
    dl->PopClipRect();

    if (holeMin.y > cardMin.y) {
        dl->PushClipRect(cardMin, ImVec2(holeMax.x, holeMin.y), true);
        dl->AddRectFilled(cardMin, cardMax, fillCol, r, ImDrawFlags_RoundCornersTopLeft);
        dl->PopClipRect();
    }

    dl->PushClipRect(ImVec2(cardMin.x, holeMin.y), ImVec2(holeMin.x, holeMax.y), true);
    dl->AddRectFilled(cardMin, cardMax, fillCol, 0.0f, ImDrawFlags_None);
    dl->PopClipRect();

    if (holeMax.y < cardMax.y) {
        dl->PushClipRect(ImVec2(cardMin.x, holeMax.y), ImVec2(holeMax.x, cardMax.y), true);
        dl->AddRectFilled(cardMin, cardMax, fillCol, r, ImDrawFlags_RoundCornersBottomLeft);
        dl->PopClipRect();
    }

    // BORDER
    if ((borderCol >> IM_COL32_A_SHIFT) > 0) {
        dl->PushClipRect(ImVec2(holeMax.x, cardMin.y), cardMax, true);
        dl->AddRect(cardMin, cardMax, borderCol, r, ImDrawFlags_RoundCornersRight, style.borderThickness);
        dl->PopClipRect();

        if (holeMin.y > cardMin.y) {
            dl->PushClipRect(cardMin, ImVec2(holeMax.x, holeMin.y), true);
            dl->AddRect(cardMin, cardMax, borderCol, r, ImDrawFlags_RoundCornersTopLeft, style.borderThickness);
            dl->PopClipRect();
        }

        dl->PushClipRect(ImVec2(cardMin.x, holeMin.y), ImVec2(holeMin.x, holeMax.y), true);
        dl->AddRect(cardMin, cardMax, borderCol, 0.0f, ImDrawFlags_None, style.borderThickness);
        dl->PopClipRect();

        if (holeMax.y < cardMax.y) {
            dl->PushClipRect(ImVec2(cardMin.x, holeMax.y), ImVec2(holeMax.x, cardMax.y), true);
            dl->AddRect(cardMin, cardMax, borderCol, r, ImDrawFlags_RoundCornersBottomLeft, style.borderThickness);
            dl->PopClipRect();
        }
    }
}