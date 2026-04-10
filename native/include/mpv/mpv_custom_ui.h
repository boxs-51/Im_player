#pragma once
#include "utils.h"
#include "globals.h"

#include <mpv/mpv_settings.h>

#include <imgui_internal.h>
#include <imgui.h>
#include <string>
#include <stdarg.h>
#include <map>
#include <regex>


static inline ImVec2 Lerp(const ImVec2& a, const ImVec2& b, float t)
{
    return ImVec2(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    );
}

// t in [0..1]: 0=Play, 1=Pause
struct PlayPauseData
{
    float t = 0.0f;      // animation 0→1
    bool paused = false; // state thực của player

    bool hovered = false; // trạng thái hover hiện tại
};

inline static void DrawPlayPauseIcon(
    ImDrawList* dl,
    ImVec2 pMin,
    ImVec2 pMax,
    ImU32 color,
    void* user_data)
{
    PlayPauseData* data = (PlayPauseData*)user_data;
    if (!data) return;

    // 1. Cập nhật t mượt mà hơn (dùng dt nếu có thể, hoặc 0.15f cho cảm giác mượt)
    float target = data->paused ? 1.0f : 0.0f;
    data->t = ImLerp(data->t, target, 0.15f); 
    float t = ImClamp(data->t, 0.0f, 1.0f);

    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;
    
    // Tăng padding để icon "thoáng" và sang hơn
    float paddingX = w * 0.20f;
    float paddingY = h * 0.18f;
    float barW = w * 0.25f; // Độ rộng thanh mỏng hơn nhìn sẽ tinh tế hơn
    float gap = w * 0.10f;  // Khoảng cách giữa 2 thanh khi là Pause

    // --- TỌA ĐỘ PLAY (Tam giác) ---
    ImVec2 a1 = ImVec2(pMin.x + paddingX, pMin.y + paddingY);
    ImVec2 a2 = ImVec2(pMin.x + paddingX, pMax.y - paddingY);
    ImVec2 a3 = ImVec2(pMax.x - paddingX, pMin.y + h * 0.5f);

    // --- TỌA ĐỘ PAUSE (Thanh thứ 1) ---
    ImVec2 b1 = ImVec2(pMin.x + paddingX, pMin.y + paddingY);
    ImVec2 b2 = ImVec2(pMin.x + paddingX, pMax.y - paddingY);
    ImVec2 b3 = ImVec2(pMin.x + paddingX + barW, pMin.y + paddingY);
    ImVec2 b4 = ImVec2(pMin.x + paddingX + barW, pMax.y - paddingY);

    // Lerp tọa độ thanh 1
    ImVec2 p1 = ImLerp(a1, b1, t);
    ImVec2 p2 = ImLerp(a2, b2, t);
    ImVec2 p3 = ImLerp(a3, b3, t);
    ImVec2 p4 = ImLerp(a3, b4, t);

    // Vẽ thanh 1 (Biến thể từ tam giác sang chữ nhật)
    dl->AddQuadFilled(p1, p3, p4, p2, color);

    // --- THANH THỨ 2 (Chỉ vẽ khi t > 0.01 để tránh vệt mờ) ---
    if (t > 0.01f)
    {
        // Alpha mượt: chỉ bắt đầu hiện rõ sau khi t > 0.3
        float alphaFactor = ImClamp((t - 0.2f) / 0.8f, 0.0f, 1.0f);
        ImU32 baseColorAlpha = (color >> 24) & 0xFF;
        ImU32 newAlpha = (ImU32)(baseColorAlpha * alphaFactor);
        ImU32 color2 = (color & 0x00FFFFFF) | (newAlpha << 24);

        // Vị trí thanh 2: chạy từ vị trí thanh 1 ra vị trí cuối
        // Khi t=0, thanh 2 nằm đè lên thanh 1, khi t=1, nó cách 1 đoạn gap
        float currentGap = t * (barW + gap);
        ImVec2 bar2_min = ImVec2(p1.x + currentGap, pMin.y + paddingY);
        ImVec2 bar2_max = ImVec2(p3.x + currentGap, pMax.y - paddingY);

        if (newAlpha > 0) {
            dl->AddRectFilled(bar2_min, bar2_max, color2, 1.0f); // bo góc nhẹ 1.0f cho sang
        }
    }
}
inline static void DrawPlayIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    // Vẽ tam giác cho nút Play
    ImVec2 p1 = pMin;
    ImVec2 p2 = ImVec2(pMin.x, pMax.y);
    ImVec2 p3 = ImVec2(pMax.x, (pMin.y + pMax.y) * 0.5f);
    drawList->AddTriangleFilled(p1, p2, p3, color);
}

inline static void DrawPauseIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float width = pMax.x - pMin.x;
    float barWidth = width * 0.35f;
    // Thanh bên trái
    drawList->AddRectFilled(pMin, ImVec2(pMin.x + barWidth, pMax.y), color);
    // Thanh bên phải
    drawList->AddRectFilled(ImVec2(pMax.x - barWidth, pMin.y), pMax, color);
}

inline static void DrawPrevIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float width = pMax.x - pMin.x;
    float barWidth = width * 0.15f;
    // Thanh đứng bên trái
    drawList->AddRectFilled(pMin, ImVec2(pMin.x + barWidth, pMax.y), color);
    // Tam giác hướng trái
    ImVec2 p1 = ImVec2(pMax.x, pMin.y);
    ImVec2 p2 = ImVec2(pMax.x, pMax.y);
    ImVec2 p3 = ImVec2(pMin.x + barWidth, (pMin.y + pMax.y) * 0.5f);
    drawList->AddTriangleFilled(p1, p2, p3, color);
}

inline static void DrawNextIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float width = pMax.x - pMin.x;
    float barWidth = width * 0.15f;
    // Tam giác hướng phải
    ImVec2 p1 = pMin;
    ImVec2 p2 = ImVec2(pMin.x, pMax.y);
    ImVec2 p3 = ImVec2(pMax.x - barWidth, (pMin.y + pMax.y) * 0.5f);
    drawList->AddTriangleFilled(p1, p2, p3, color);
    // Thanh đứng bên phải
    drawList->AddRectFilled(ImVec2(pMax.x - barWidth, pMin.y), pMax, color);
}
struct VolumeIconData
{
    int   volume = 100;
    bool  isMuted = false;

    float waveT   = 1.0f; // 0..1 : độ hiện sóng
    float muteT   = 0.0f; // 0..1 : độ hiện dấu X
};
inline static void DrawVolumeIcon(
    ImDrawList* drawList,
    ImVec2 pMin,
    ImVec2 pMax,
    ImU32 color,
    void* user_data)
{
    VolumeIconData* data = (VolumeIconData*)user_data;
    if (!data) return;

    int volume = data->volume;
    bool isMuted = data->isMuted || volume == 0;

    // =========================
    // Animation update
    // =========================
    float waveTarget = isMuted ? 0.0f : 1.0f;
    float muteTarget = isMuted ? 1.0f : 0.0f;

    data->waveT = ImLerp(data->waveT, waveTarget, 0.15f);
    data->muteT = ImLerp(data->muteT, muteTarget, 0.2f);

    float waveT = ImClamp(data->waveT, 0.0f, 1.0f);
    float muteT = ImClamp(data->muteT, 0.0f, 1.0f);

    // =========================
    // Geometry
    // =========================
    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;
    float centerY = pMin.y + h * 0.5f;

    //
    // 1. Speaker box
    //
    float boxW = w * 0.25f;
    float boxH = h * 0.3f;

    ImVec2 speakerBoxMin(pMin.x, centerY - boxH * 0.5f);
    ImVec2 speakerBoxMax(pMin.x + boxW, centerY + boxH * 0.5f);

    drawList->AddRectFilled(speakerBoxMin, speakerBoxMax, color);

    //
    // 2. Speaker cone
    //
    ImVec2 pts[4] = {
        ImVec2(speakerBoxMax.x, speakerBoxMin.y),
        ImVec2(speakerBoxMax.x, speakerBoxMax.y),
        ImVec2(pMin.x + w * 0.5f, pMax.y - h * 0.15f),
        ImVec2(pMin.x + w * 0.5f, pMin.y + h * 0.15f),
    };
    drawList->AddConvexPolyFilled(pts, 4, color);

    // =========================
    // 3. Mute X (fade)
    // =========================
    if (muteT > 0.01f)
    {
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

    // =========================
    // 4. Waves (scale + fade)
    // =========================
    if (waveT > 0.01f)
    {
        int waves = (volume > 66) ? 3 :
                    (volume > 33) ? 2 :
                    1;

        ImU32 baseA = (color >> 24) & 0xFF;
        ImU32 a = (ImU32)(baseA * waveT);
        ImU32 waveColor = (color & 0x00FFFFFF) | (a << 24);

        for (int j = 0; j < waves; j++)
        {
            float radius =
                ((w * 0.22f) + (j * w * 0.14f)) *
                (0.85f + 0.15f * waveT);

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
struct SettingsIconData {
    bool hovered;
    bool opened;

    // internal animation state
    float hover_t = 0.0f;
    float open_t  = 0.0f;
    float angle   = 0.0f;
};
inline static void DrawSettingsIconAnimated(
    ImDrawList* drawList,
    ImVec2 pMin,
    ImVec2 pMax,
    ImU32 color,
    void* user_data)
{
    SettingsIconData* d = (SettingsIconData*)user_data;
    if (!d) return;

    // -------------------------
    // INTERNAL ANIMATION UPDATE
    // -------------------------
    float dt = ImGui::GetIO().DeltaTime;
    float speed = 8.0f;

    d->hover_t += (d->hovered ? 1.0f : -1.0f) * dt * speed;
    d->open_t  += (d->opened  ? 1.0f : -1.0f) * dt * speed;

    d->hover_t = ImClamp(d->hover_t, 0.0f, 1.0f);
    d->open_t  = ImClamp(d->open_t,  0.0f, 1.0f);

    // Xoay khi mở (mượt, không giật)
    d->angle += d->open_t * dt * 2.5f;

    // -------------------------
    // GEOMETRY
    // -------------------------
    ImVec2 center = ImVec2((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
    float w = pMax.x - pMin.x;

    float radiusOuter = w * 0.35f;
    float radiusInner = radiusOuter * 0.5f;
    float toothLen    = w * 0.1f;
    int   teeth       = 8;

    // Hover breathe (scale nhẹ)
    float breathe = 1.0f + sinf(d->hover_t * IM_PI * 2.0f) * 0.04f;
    radiusOuter *= breathe;
    radiusInner *= breathe;

    // -------------------------
    // DRAW GEAR RING
    // -------------------------
    drawList->AddCircle(
        center,
        radiusOuter,
        color,
        24,
        w * 0.08f
    );

    // -------------------------
    // DRAW TEETH
    // -------------------------
    for (int i = 0; i < teeth; i++)
    {
        float a = d->angle + i * (IM_PI * 2.0f / teeth);
        float c = cosf(a);
        float s = sinf(a);

        ImVec2 p1(
            center.x + c * (radiusOuter - w * 0.05f),
            center.y + s * (radiusOuter - w * 0.05f)
        );

        ImVec2 p2(
            center.x + c * (radiusOuter + toothLen),
            center.y + s * (radiusOuter + toothLen)
        );

        drawList->AddLine(p1, p2, color, w * 0.12f);
    }

    // -------------------------
    // DRAW CENTER AXIS
    // -------------------------
    drawList->AddCircleFilled(
        center,
        radiusInner * 0.6f,
        color
    );
}

inline static bool TogglePopupFullscreen(HWND hwnd)
{
    /*
    if (!hwnd)
        return false;

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (!(style & WS_POPUP))
        return false; // chỉ popup mới xử lý
    */
    /* ===============================
       RESTORE
       =============================== */
    /*
    if (overlaydata.isOverlayFullscreen)
    {
        if (overlaydata.hasRestoreRc)
        {
            SetWindowPos(
                hwnd,
                HWND_NOTOPMOST,
                overlaydata.OverlayRestoreRc.left,
                overlaydata.OverlayRestoreRc.top,
                overlaydata.OverlayRestoreRc.right  - overlaydata.OverlayRestoreRc.left,
                overlaydata.OverlayRestoreRc.bottom - overlaydata.OverlayRestoreRc.top,
                SWP_FRAMECHANGED
            );
        }

        overlaydata.isOverlayFullscreen = false;
        return false;
    }
    */
    /* ===============================
       ENTER FULLSCREEN
       =============================== */
    /*
    // cache rect hiện tại
    GetWindowRect(hwnd, &overlaydata.OverlayRestoreRc);
    overlaydata.hasRestoreRc = true;

    // lấy monitor gần nhất
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMon, &mi);

    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        mi.rcMonitor.left,
        mi.rcMonitor.top,
        mi.rcMonitor.right  - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_FRAMECHANGED
    );

    overlaydata.isOverlayFullscreen = true;
    return true;
    */
}

// 2. Icon Phóng to (Fullscreen)
struct FullscreenIconData
{
    bool fullscreen = false;
    float t = 0.0f; // 0..1 animation
};

inline static void DrawFullscreenIconAnimated(
    ImDrawList* drawList,
    ImVec2 pMin,
    ImVec2 pMax,
    ImU32 color,
    void* user_data)
{
    FullscreenIconData* data = (FullscreenIconData*)user_data;
    if (!data) return;

    // ======================
    // Animation update
    // ======================
    float target = data->fullscreen ? 1.0f : 0.0f;
    data->t = ImLerp(data->t, target, 0.18f);
    float t = ImClamp(data->t, 0.0f, 1.0f);

    // ======================
    // Geometry
    // ======================
    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;
    float lineLen = w * 0.25f;
    float thick = 2.0f;

    // inner padding khi unfullscreen
    float inset = ImLerp(0.0f, w * 0.12f, t);

    ImVec2 aMin = ImVec2(pMin.x + inset, pMin.y + inset);
    ImVec2 aMax = ImVec2(pMax.x - inset, pMax.y - inset);

    // ======================
    // Alpha nhẹ cho cảm giác mềm
    // ======================
    ImU32 baseA = (color >> 24) & 0xFF;
    ImU32 alpha = (ImU32)(baseA * (0.85f + 0.15f * (1.0f - t)));
    ImU32 col = (color & 0x00FFFFFF) | (alpha << 24);

    // ======================
    // Draw 4 corners
    // ======================

    // Top-left
    drawList->AddLine(
        aMin,
        ImVec2(aMin.x + lineLen, aMin.y),
        col, thick);
    drawList->AddLine(
        aMin,
        ImVec2(aMin.x, aMin.y + lineLen),
        col, thick);

    // Top-right
    drawList->AddLine(
        ImVec2(aMax.x, aMin.y),
        ImVec2(aMax.x - lineLen, aMin.y),
        col, thick);
    drawList->AddLine(
        ImVec2(aMax.x, aMin.y),
        ImVec2(aMax.x, aMin.y + lineLen),
        col, thick);

    // Bottom-left
    drawList->AddLine(
        ImVec2(aMin.x, aMax.y),
        ImVec2(aMin.x + lineLen, aMax.y),
        col, thick);
    drawList->AddLine(
        ImVec2(aMin.x, aMax.y),
        ImVec2(aMin.x, aMax.y - lineLen),
        col, thick);

    // Bottom-right
    drawList->AddLine(
        aMax,
        ImVec2(aMax.x - lineLen, aMax.y),
        col, thick);
    drawList->AddLine(
        aMax,
        ImVec2(aMax.x, aMax.y - lineLen),
        col, thick);
}
inline static void DrawFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float w = pMax.x - pMin.x;
    float lineLen = w * 0.25f; // Độ dài cạnh góc vuông
    float thick = 2.0f;

    // Góc trên trái
    drawList->AddLine(pMin, ImVec2(pMin.x + lineLen, pMin.y), color, thick);
    drawList->AddLine(pMin, ImVec2(pMin.x, pMin.y + lineLen), color, thick);

    // Góc trên phải
    drawList->AddLine(ImVec2(pMax.x, pMin.y), ImVec2(pMax.x - lineLen, pMin.y), color, thick);
    drawList->AddLine(ImVec2(pMax.x, pMin.y), ImVec2(pMax.x, pMin.y + lineLen), color, thick);

    // Góc dưới trái
    drawList->AddLine(ImVec2(pMin.x, pMax.y), ImVec2(pMin.x + lineLen, pMax.y), color, thick);
    drawList->AddLine(ImVec2(pMin.x, pMax.y), ImVec2(pMin.x, pMax.y - lineLen), color, thick);

    // Góc dưới phải
    drawList->AddLine(pMax, ImVec2(pMax.x - lineLen, pMax.y), color, thick);
    drawList->AddLine(pMax, ImVec2(pMax.x, pMax.y - lineLen), color, thick);
}

// 3. Icon Thu nhỏ (Un-Fullscreen - Thoát toàn màn hình)
inline static void DrawUnFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;
    float lineLen = w * 0.25f;
    float thick = 2.0f;
    
    // Tính toán lại pMin, pMax để icon nhỏ hơn chút, tạo cảm giác "thu vào"
    ImVec2 cMin = ImVec2(pMin.x + w*0.1f, pMin.y + h*0.1f);
    ImVec2 cMax = ImVec2(pMax.x - w*0.1f, pMax.y - h*0.1f);

    // Vẽ các góc hướng vào trong (ngược với Fullscreen) hoặc vẽ 2 hình chữ nhật lồng nhau
    // Cách đơn giản: Vẽ 4 góc nhưng quay ngược lại
    // Tuy nhiên icon Unfullscreen chuẩn thường là 2 góc đối diện ép vào nhau.
    // Dưới đây là vẽ 4 góc ép vào tâm:
    
    // Góc trên trái (hướng vào tâm)
    drawList->AddLine(cMin, ImVec2(cMin.x + lineLen, cMin.y), color, thick);
    drawList->AddLine(cMin, ImVec2(cMin.x, cMin.y + lineLen), color, thick);
    
    // Góc dưới phải (đối xứng)
    drawList->AddLine(cMax, ImVec2(cMax.x - lineLen, cMax.y), color, thick);
    drawList->AddLine(cMax, ImVec2(cMax.x, cMax.y - lineLen), color, thick);
    
    // Vẽ thêm 1 hình chữ nhật nhỏ mờ hoặc viền ở giữa nếu muốn, nhưng 2 góc trên là đủ hiểu.
}
struct OptionIconData {
    bool hovered;  
    bool opened;   


    // internal animation state (hàm tự xử)
    float hover_t = 0.0f;
    float open_t  = 0.0f;
};
// 4. Icon Option (Dấu 3 chấm dọc hoặc ngang)
inline static void DrawOptionIconAnimated(
    ImDrawList* drawList,
    ImVec2 pMin,
    ImVec2 pMax,
    ImU32 color,
    void* user_data)
{
    OptionIconData* d = (OptionIconData*)user_data;
    if (!d) return;

    // ---- INTERNAL TIMING ----
    float dt = ImGui::GetIO().DeltaTime;
    float speed = 4.0f; // chậm lại để thấy rõ 1 lần

    // ===== HOVER ONE-SHOT LOGIC =====
    if (d->hovered)
    {
        if (d->hover_t < 1.0f)
            d->hover_t += dt * speed;
    }
    else
    {
        // reset khi rời hover
        d->hover_t = 0.0f;
    }

    d->hover_t = ImClamp(d->hover_t, 0.0f, 1.0f);

    // Open anim vẫn giữ như cũ
    d->open_t += (d->opened ? 1.0f : -1.0f) * dt * 8.0f;
    d->open_t = ImClamp(d->open_t, 0.0f, 1.0f);

    float ht = d->hover_t;
    float ot = d->open_t;

    // ---- GEOMETRY ----
    float w = pMax.x - pMin.x;
    float h = pMax.y - pMin.y;

    float baseR = w * 0.08f;
    float cx = pMin.x + w * 0.5f;

    // ===== OPEN MORPH =====
    float stretch = ImLerp(1.0f, 3.0f, ot);

    float y[3] = {
        pMin.y + h * 0.25f,
        pMin.y + h * 0.50f,
        pMin.y + h * 0.75f
    };

    // ===== HOVER SLIDE (ONE-SHOT + PHASE SHIFT) =====
    float slideAmount = w * 0.12f;
    float phaseStep  = 0.4f; // lệch pha đẹp cho 3 chấm

    for (int i = 0; i < 3; i++)
    {
        float phase = i * phaseStep;

        float envelope = sinf(ht * IM_PI);          // 0 → 1 → 0
        float wave     = sinf(ht * IM_PI + phase); // lệch pha

        // sin(ht * PI) => đi ra rồi quay về
        float slide = envelope * wave * slideAmount;

        // nếu đang open thì không slide
        if (d->opened)
            slide = 0.0f;

        drawList->AddEllipseFilled(
            ImVec2(cx + slide, y[i]),
            ImVec2(baseR * stretch, baseR),
            color
        );
    }
}

struct LoadingIconData {

    ImDrawList* drawList;

    float angle = 0.0f;
    float speed = 4.0f;
    
    // Thêm các trường này
    ImVec2 pos = ImVec2(0, 0);  // Tọa độ góc trên bên trái (Screen Space)
    ImVec2 size = ImVec2(0, 0); // Kích thước vùng vẽ
};

inline static void DrawLoadingIconAnimated(
    ImDrawList* drawList,
    ImVec2 pMin, // Sẽ được tính lại dựa trên data
    ImVec2 pMax, // Sẽ được tính lại dựa trên data
    ImU32 color,
    void* user_data)
{
    LoadingIconData* d = (LoadingIconData*)user_data;
    if (!d) return;

    if (d->drawList)ImDrawList* drawList = d->drawList;

    // Cập nhật Animation
    float dt = ImGui::GetIO().DeltaTime;
    d->angle += dt * d->speed;
    if (d->angle > IM_PI * 2.0f) d->angle -= IM_PI * 2.0f;

    // --- TÍNH TOÁN LẠI VỊ TRÍ DỰA TRÊN DATA ---
    // Nếu d->size.x > 0, chúng ta dùng data. Nếu không, dùng pMin/pMax mặc định từ Button
    ImVec2 finalMin = (d->size.x > 0) ? d->pos : pMin;
    ImVec2 finalMax = (d->size.x > 0) ? (d->pos + d->size) : pMax;

    ImVec2 center = ImVec2((finalMin.x + finalMax.x) * 0.5f, (finalMin.y + finalMax.y) * 0.5f);
    float radius = (finalMax.x - finalMin.x) * 0.4f;
    float thickness = (finalMax.x - finalMin.x) * 0.1f;

    // Vẽ vòng nền mờ
    ImU32 bgColor = (color & 0x00FFFFFF) | (0x33 << 24);
    drawList->AddCircle(center, radius, bgColor, 30, thickness);

    // Vẽ Loading arc
    float arcLength = IM_PI * 0.5f + (sinf(d->angle) * 0.5f + 0.5f) * IM_PI;
    
    drawList->PathArcTo(center, radius, d->angle, d->angle + arcLength, 30);
    drawList->PathStroke(color, false, thickness);
}

struct SeekingIconData {
    float timer = 0.0f;
    float alpha = 0.0f;   
    float pulse = 0.0f;   
    bool  forward = true;

    ImVec2 pos = ImVec2(0, 0);  
    ImVec2 size = ImVec2(0, 0);
    
    bool g_isSeeking =false;
};
inline static void DrawSeekingIconAnimated(
    ImDrawList* drawList,
    ImVec2 pMin,
    ImVec2 pMax,
    ImU32 color,
    void* user_data)
{
    SeekingIconData* d = (SeekingIconData*)user_data;
    // Kiểm tra an toàn và ngưỡng hiển thị
    if (!d || d->alpha <= 0.001f) return;

    // 1. Xác định không gian vẽ (Item space vs Video space)
    ImVec2 center;
    float fullW, fullH;

    if (d->size.x > 0.0f && d->size.y > 0.0f) {
        // Ưu tiên dùng tọa độ video nếu có
        if(d->forward)
            center = ImVec2(d->pos.x + d->size.x * 0.75f, d->pos.y + d->size.y * 0.5f);
        else 
            center = ImVec2(d->pos.x + d->size.x * 0.25f, d->pos.y + d->size.y * 0.5f);
        fullW = d->size.x;
        fullH = d->size.y;
    } else {
        // Fallback về vùng của ImGui Item
        center = ImVec2((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
        fullW = pMax.x - pMin.x;
        fullH = pMax.y - pMin.y;
    }

    float dir = d->forward ? 1.0f : -1.0f;
    float triW = fullW * 0.05f; // Thu nhỏ lại một chút cho cân đối
    float triH = fullH * 0.20f;
    float spacing = triW * 1.0f;

    float speed = 1.5f;
    float t = fmodf(d->timer * speed, 1.0f);

    // Chuẩn bị màu sắc
    ImVec4 colVec = ImGui::ColorConvertU32ToFloat4(color);
    // Tạo màu trắng pha (Sáng hơn màu gốc)
    ImVec4 brightCol = ImVec4(
        ImMin(colVec.x + 0.3f, 1.0f), 
        ImMin(colVec.y + 0.3f, 1.0f), 
        ImMin(colVec.z + 0.3f, 1.0f), 
        colVec.w * d->alpha
    );
    float baseAlpha = brightCol.w;

    ImU32 bgGlowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(brightCol.x, brightCol.y, brightCol.z, baseAlpha * 0.15f));
    ImU32 transparent = ImGui::ColorConvertFloat4ToU32(ImVec4(brightCol.x, brightCol.y, brightCol.z, 0.0f));

    if (d->forward) {
        drawList->AddRectFilledMultiColor(
            ImVec2(d->pos.x + d->size.x * 0.5f, d->pos.y), 
            ImVec2(d->pos.x + d->size.x, d->pos.y + d->size.y),
            transparent, bgGlowCol, bgGlowCol, transparent
        );
    } else {
        drawList->AddRectFilledMultiColor(
            ImVec2(d->pos.x, d->pos.y), 
            ImVec2(d->pos.x + d->size.x * 0.5f, d->pos.y + d->size.y),
            bgGlowCol, transparent, transparent, bgGlowCol
        );
    }

    int glowLayers = 6; // Giảm xuống 6 để tối ưu hiệu năng
    float maxGlowRadius = triH * 0.7f;

    for (int i = 0; i < 3; i++) {
        float offset = ((float)i + t) * spacing;
        float x = center.x + (offset - (spacing * 1.5f)) * dir;
        ImVec2 glowPos = ImVec2(x + (dir > 0 ? triW * 0.4f : -triW * 0.4f), center.y);

        for (int layer = 1; layer <= glowLayers; layer++) {
            float fraction = (float)layer / (float)glowLayers;
            float r = maxGlowRadius * fraction;
            // Độ mờ layer: giảm dần khi ra xa tâm
            float lAlpha = (1.0f - fraction) * 0.3f * baseAlpha; 
            
            if (lAlpha <= 0.0f) continue;
            
            ImU32 gCol = ImGui::ColorConvertFloat4ToU32(ImVec4(colVec.x, colVec.y, colVec.z, lAlpha));
            drawList->AddCircleFilled(glowPos, r, gCol, 16);
        }
    }

    for (int i = 0; i < 3; i++) {
        float offset = ((float)i + t) * spacing;
        float x = center.x + (offset - (spacing * 1.5f)) * dir;

        // Tính Fade dựa trên vị trí (0.0 -> 1.0)
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
}

struct PlayPauseOverlay {
    float alpha = 0.0f;
    float scale = 1.0f;
    bool last_paused = false; 
    bool initialized = false;
};
inline void DrawGhostStatusOverlay(ImVec2 vPos, ImVec2 vSize, bool isPaused) {
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
struct IconButtonStyle
{
    // Button background: Dùng Alpha thấp cho cảm giác "Glassmorphism"
    bool drawButtonBg = true;
    ImU32 buttonBgColor      = IM_COL32(255, 255, 255, 20);  // Rất mờ, chỉ đủ thấy lớp nền
    ImU32 buttonBgHovered    = IM_COL32(255, 255, 255, 45);  // Sáng nhẹ lên khi hover
    ImU32 buttonBgActive     = IM_COL32(255, 255, 255, 65);  // Đậm hơn chút khi nhấn
    float buttonRounding     = 8.0f;                         // Bo góc tròn hơn nhìn hiện đại hơn

    // Button border: Viền cực mảnh và mờ để định hình khối
    bool drawButtonBorder = true; 
    ImU32 buttonBorderColor = IM_COL32(255, 255, 255, 30);   
    float buttonBorderThickness = 1.0f;

    // Icon background
    bool drawIconBg = false;
    ImU32 iconBgColor = IM_COL32(0, 0, 0, 40);
    float iconBgRounding = 4.0f;

    // Icon border
    bool drawIconBorder = false;
    ImU32 iconBorderColor = IM_COL32(255, 255, 255, 160);
    float iconBorderThickness = 1.2f;


    // Icon: Tránh dùng màu cam/vàng gắt, dùng màu trắng/xanh nhạt sang trọng hơn
    ImU32 iconNormal   = IM_COL32(230, 230, 230, 200); // Hơi mờ ở trạng thái nghỉ
    ImU32 iconHovered  = IM_COL32(255, 255, 255, 255); // Sáng rực hoàn toàn khi hover
    ImU32 iconActive   = IM_COL32(180, 210, 255, 255); // Màu xanh nhạt nhẹ khi nhấn (cảm giác công nghệ)

    // Icon animation strength (scale)
    float iconHoverScale = 1.05f;
    float iconActiveScale = 0.95f;
};
inline const IconButtonStyle& GetDefaultIconButtonStyle()
{
    static IconButtonStyle s;
    return s;
}
inline static bool CustomIconButton(
    const char* str_id,
    void(*drawFn)(ImDrawList*, ImVec2, ImVec2, ImU32, void*),
    ImVec2 size,
    void* user_data,
    const IconButtonStyle& style
){
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGuiID id = window->GetID(str_id);
    ImRect bb(pos, pos + size);

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    //
    // ---------- BUTTON BACKGROUND ----------
    //
    if (style.drawButtonBg)
    {
        ImU32 bg =
            held   ? style.buttonBgActive :
            hovered? style.buttonBgHovered :
                     style.buttonBgColor;

        dl->AddRectFilled(
            bb.Min,
            bb.Max,
            bg,
            style.buttonRounding
        );
    }

    //
    // ---------- BUTTON BORDER ----------
    //
    if (style.drawButtonBorder)
    {
        dl->AddRect(
            bb.Min,
            bb.Max,
            style.buttonBorderColor,
            style.buttonRounding,
            0,
            style.buttonBorderThickness
        );
    }

    //
    // ---------- ICON AREA ----------
    //
    float padding = size.x * 0.2f;

    ImVec2 iconMin = pos + ImVec2(padding, padding);
    ImVec2 iconMax = pos + size - ImVec2(padding, padding);

    // scale effect
    float scale =
        held   ? style.iconActiveScale :
        hovered? style.iconHoverScale  :
                 1.0f;

    ImVec2 center = (iconMin + iconMax) * 0.5f;
    ImVec2 halfSize = (iconMax - iconMin) * 0.5f * scale;

    iconMin = center - halfSize;
    iconMax = center + halfSize;

    //
    // ---------- ICON BACKGROUND ----------
    //
    if (style.drawIconBg)
    {
        dl->AddRectFilled(
            iconMin,
            iconMax,
            style.iconBgColor,
            style.iconBgRounding
        );
    }

    //
    // ---------- ICON BORDER ----------
    //
    if (style.drawIconBorder)
    {
        dl->AddRect(
            iconMin,
            iconMax,
            style.iconBorderColor,
            style.iconBgRounding,
            0,
            style.iconBorderThickness
        );
    }

    //
    // ---------- ICON DRAW ----------
    //
    ImU32 iconColor = ImGui::GetColorU32(
        held ? style.iconActive :
        hovered ? style.iconHovered :
        style.iconNormal
    );

    drawFn(
        dl,
        iconMin,
        iconMax,
        iconColor,
        user_data
    );

    return pressed;
}
using OldIconFn = void(*)(ImDrawList*, ImVec2, ImVec2, ImU32);
inline void IconWrapperAdapter(
    ImDrawList* dl,
    ImVec2 min,
    ImVec2 max,
    ImU32 col,
    void* user_data)
{
    OldIconFn fn = reinterpret_cast<OldIconFn>(user_data);
    fn(dl, min, max, col);
}
inline bool CustomIconButton(
    const char* str_id,
    void(*drawFn)(ImDrawList*, ImVec2, ImVec2, ImU32, void*),
    ImVec2 size)
{
    return CustomIconButton(
        str_id,
        drawFn,
        size,
        nullptr,
        GetDefaultIconButtonStyle()
    );
}
inline bool CustomIconButton(
    const char* str_id,
    OldIconFn oldFn,
    ImVec2 size)
{
    return CustomIconButton(
        str_id,
        IconWrapperAdapter,
        size,
        reinterpret_cast<void*>(oldFn),
        GetDefaultIconButtonStyle()
    );
}
inline bool CustomIconButton(
    const char* str_id,
    void(*drawFn)(ImDrawList*, ImVec2, ImVec2, ImU32, void*),
    ImVec2 size,
    void* user_data)
{
    return CustomIconButton(
        str_id,
        drawFn,
        size,
        user_data,
        GetDefaultIconButtonStyle()
    );
}

// Helper: format một argument an toàn
inline std::string safeFormatArg(const char* fmtSpec, va_list args, char type) {
    std::string result;
    char tmp[128]; // tạm để snprintf

    switch (type) {
        case 's': {
            const char* str = va_arg(args, const char*);
            if (!str || str[0] == '\0') str = "None";
            int n = snprintf(tmp, sizeof(tmp), fmtSpec, str);
            result.assign(tmp, n);
            break;
        }
        case 'd': {
            int val = va_arg(args, int);
            int n = snprintf(tmp, sizeof(tmp), fmtSpec, val);
            result.assign(tmp, n);
            break;
        }
        case 'u': {
            unsigned int val = va_arg(args, unsigned int);
            int n = snprintf(tmp, sizeof(tmp), fmtSpec, val);
            result.assign(tmp, n);
            break;
        }
        case 'f': {
            double val = va_arg(args, double);
            int n = snprintf(tmp, sizeof(tmp), fmtSpec, val);
            result.assign(tmp, n);
            break;
        }
        case 'p': {
            void* ptr = va_arg(args, void*);
            int n = snprintf(tmp, sizeof(tmp), fmtSpec, ptr);
            result.assign(tmp, n);
            break;
        }
        case '%': {
            result = "%";
            break;
        }
        default:
            break;
    }

    return result;
}
// Khai báo biến toàn cục để các Helper sử dụng

struct ThemeColors {
    
    //Theme Modern Window Style
    ImVec4 Text_ModernWindowStyle;
    ImVec4 WindowBg_ModernWindowStyle;
    ImVec4 TitleBg_ModernWindowStyle;
    ImVec4 TitleBgActive_ModernWindowStyle;
    ImVec4 Border_ModernWindowStyle;
    ImVec4 Separator_ModernWindowStyle;

    //Theme _Modern Child
    ImVec4 ChildBg_ModernChild;
    ImVec4 Border_ModernChild;
    ImVec4 Text_ModernChild;

    //Theme Card
    ImVec4 ChildBg_Card;
    ImVec4 Border_Card;
    ImVec4 Text_Card;

    //Theme Info Table
    ImVec4 TableRowBg_InfoTable;
    ImVec4 TableRowBgAlt_InfoTable;

    //Theme Modern TabBar
    ImVec4 Text_ModernTabBar;
    ImVec4 Tab_ModernTabBar;
    ImVec4 TabHovered_ModernTabBar;
    ImVec4 TabActive_ModernTabBar;
    ImVec4 TabUnfocused_ModernTabBar;
    ImVec4 TabUnfocusedActive_ModernTabBar;

    //Theme Modern Button
    ImVec4 Button_ModernButton;
    ImVec4 ButtonHovered_ModernButton;
    ImVec4 ButtonActive_ModernButton;
    ImVec4 Text_ModernButton;

    //Theme Secondary Button
    ImVec4 Button_SecondaryButton;
    ImVec4 ButtonHovered_SecondaryButton;
    ImVec4 ButtonActive_SecondaryButton;
    ImVec4 Border_SecondaryButton;
    ImVec4 Text_SecondaryButton;


    ImVec4 Text_ModernCheckbox;
    ImVec4 FrameBg_ModernCheckbox;
    ImVec4 FrameBgHovered_ModernCheckbox;
    ImVec4 FrameBgActive_ModernCheckbox;
    ImVec4 CheckMark_ModernCheckbox;

    ImVec4 FrameBg_ModernInputTextMultiline;
    ImVec4 FrameBgHovered_ModernInputTextMultiline;
    ImVec4 FrameBgActive_ModernInputTextMultiline;
    ImVec4 Border_ModernInputTextMultiline;
    ImVec4 TextSelectedBg_ModernInputTextMultiline;
    ImVec4 Text_ModernInputTextMultiline;

    ImVec4 Header_ModernSelectable;
    ImVec4 HeaderHovered_ModernSelectable;
    ImVec4 HeaderActive_ModernSelectable;
    ImVec4 Text_Selected_ModernSelectable;
    ImVec4 Text_UnSelected_ModernSelectable;

    ImVec4 TableHeaderBg_ListTable;
    ImVec4 Text_ListTable;
    ImVec4 TableRowBgAlt_ListTable;
    ImVec4 HeaderActive_ListTable;
    ImVec4 HeaderHovered_ListTable;

    ImVec4 Header_ModernCollapsingHeader;
    ImVec4 HeaderHovered_ModernCollapsingHeader;
    ImVec4 HeaderActive_ModernCollapsingHeader;
    ImVec4 Text_ModernCollapsingHeader;


    //Theme Normal Combo
    ImVec4 FrameBg_NormalCombo;             
    ImVec4 FrameBgHovered_NormalCombo;
    ImVec4 Border_NormalCombo;
    ImVec4 Text_NormalCombo;
    ImVec4 TextLaBel_NormalCombo;
    ImVec4 PopupBg_NormalCombo;

    ImVec4 Header_ModernTreeNode;
    ImVec4 HeaderHovered_ModernTreeNode;
    ImVec4 HeaderActive_ModernTreeNode;
    ImVec4 Text_ModernTreeNode;
};
extern ThemeColors GTheme;
static inline void SetDarkTheme() {
 
    GTheme.Text_ModernWindowStyle           = ImVec4(1.0f, 1.0f, 1.0f, 0.95f);
    GTheme.WindowBg_ModernWindowStyle       = ImVec4(0.10f, 0.10f, 0.12f, 0.95f);
    GTheme.TitleBg_ModernWindowStyle        = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    GTheme.TitleBgActive_ModernWindowStyle  = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    GTheme.Border_ModernWindowStyle         = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    GTheme.Separator_ModernWindowStyle      = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);

    GTheme.ChildBg_ModernChild  = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    GTheme.Border_ModernChild   = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    GTheme.Text_ModernChild     = ImVec4(1.0f,1.0f,1.0f,1.0f);

    GTheme.ChildBg_Card = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
    GTheme.Border_Card  = ImVec4(0.30f, 0.30f, 0.33f, 1.0f);
    GTheme.Text_Card    = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);

    GTheme.TableRowBg_InfoTable     = ImVec4(0, 0, 0, 0);
    GTheme.TableRowBgAlt_InfoTable  = ImVec4(1, 1, 1, 0.04f);

    GTheme.Text_ModernTabBar                = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    GTheme.Tab_ModernTabBar                 = ImVec4(0, 0, 0, 0);
    GTheme.TabHovered_ModernTabBar          = ImVec4(0.25f, 0.25f, 0.27f, 1.0f);
    GTheme.TabActive_ModernTabBar           = ImVec4(0.15f, 0.15f, 0.17f, 1.0f);
    GTheme.TabUnfocused_ModernTabBar        = ImVec4(0, 0, 0, 0);
    GTheme.TabUnfocusedActive_ModernTabBar  = ImVec4(0.1f, 0.45f, 0.9f, 0.7f);

    GTheme.Button_ModernButton              = ImVec4(0.12f, 0.45f, 0.90f, 1.00f);
    GTheme.ButtonHovered_ModernButton       = ImVec4(0.15f, 0.55f, 1.00f, 1.0f);
    GTheme.ButtonActive_ModernButton        = ImVec4(0.10f, 0.35f, 0.80f, 1.0f);
    GTheme.Text_ModernButton                = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);

    GTheme.Text_SecondaryButton             = ImVec4(0.85f, 0.85f, 0.88f, 1.00f);
    GTheme.Button_SecondaryButton           = ImVec4(0.15f, 0.15f, 0.17f, 0.00f);
    GTheme.ButtonHovered_SecondaryButton    = ImVec4(0.22f, 0.22f, 0.25f, 0.80f);
    GTheme.ButtonActive_SecondaryButton     = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    GTheme.Border_SecondaryButton           = ImVec4(0.35f, 0.35f, 0.38f, 1.0f);

    GTheme.Text_ModernCheckbox              = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    GTheme.FrameBg_ModernCheckbox           = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
    GTheme.FrameBgHovered_ModernCheckbox    = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
    GTheme.FrameBgActive_ModernCheckbox     = ImVec4(0.15f, 0.45f, 0.90f, 0.5f);
    GTheme.CheckMark_ModernCheckbox         = ImVec4(0.12f, 0.45f, 0.90f, 1.0f);

    GTheme.FrameBg_ModernInputTextMultiline         = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    GTheme.FrameBgHovered_ModernInputTextMultiline  = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    GTheme.FrameBgActive_ModernInputTextMultiline   = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    GTheme.Border_ModernInputTextMultiline          = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    GTheme.TextSelectedBg_ModernInputTextMultiline  = ImVec4(0.10f, 0.40f, 0.75f, 0.50f);
    GTheme.Text_ModernInputTextMultiline            = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);

    GTheme.Header_ModernSelectable          = ImVec4(0.18f, 0.21f, 0.25f, 1.00f);
    GTheme.HeaderHovered_ModernSelectable   = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    GTheme.HeaderActive_ModernSelectable    = ImVec4(0.28f, 0.33f, 0.40f, 1.00f);
    GTheme.Text_Selected_ModernSelectable   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    GTheme.Text_UnSelected_ModernSelectable = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

    
    GTheme.TableHeaderBg_ListTable  = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
    GTheme.Text_ListTable           = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    GTheme.TableRowBgAlt_ListTable  = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
    GTheme.HeaderActive_ListTable   = ImVec4(0.19f, 0.20f, 0.25f, 1.00f);
    GTheme.HeaderHovered_ListTable  = ImVec4(0.19f, 0.20f, 0.25f, 1.00f);
    
    GTheme.Header_ModernCollapsingHeader        = ImVec4(0.15f, 0.15f, 0.17f, 1.0f);
    GTheme.HeaderHovered_ModernCollapsingHeader = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    GTheme.HeaderActive_ModernCollapsingHeader  = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    GTheme.Text_ModernCollapsingHeader          = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);

    //Theme Normal Combo
    GTheme.FrameBg_NormalCombo        = ImVec4(0.12f, 0.12f, 0.14f, 1.00f); 
    GTheme.FrameBgHovered_NormalCombo = ImVec4(0.18f, 0.18f, 0.21f, 1.00f); 
    GTheme.Border_NormalCombo         = ImVec4(0.25f, 0.25f, 0.28f, 1.00f); 
    GTheme.Text_NormalCombo           = ImVec4(0.90f, 0.90f, 0.92f, 1.00f); 
    GTheme.TextLaBel_NormalCombo      = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    GTheme.PopupBg_NormalCombo        = ImVec4(0.12f, 0.12f, 0.14f, 0.98f);

    GTheme.Header_ModernTreeNode        = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    GTheme.HeaderHovered_ModernTreeNode = ImVec4(0.26f, 0.31f, 0.35f, 1.00f);
    GTheme.HeaderActive_ModernTreeNode  = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    GTheme.Text_ModernTreeNode          = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
}
static inline void SetLightTheme() {

    GTheme.Text_ModernWindowStyle           = ImVec4(0.1f, 0.1f, 0.1f, 0.95f);
    GTheme.WindowBg_ModernWindowStyle       = ImVec4(0.94f, 0.94f, 0.96f, 1.00f);
    GTheme.TitleBg_ModernWindowStyle        = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    GTheme.TitleBgActive_ModernWindowStyle  = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
    GTheme.Border_ModernWindowStyle         = ImVec4(0.70f, 0.70f, 0.75f, 1.00f);
    GTheme.Separator_ModernWindowStyle      = ImVec4(0.75f, 0.75f, 0.78f, 1.00f);

    GTheme.ChildBg_ModernChild  = ImVec4(0.97f, 0.97f, 0.98f, 1.0f);
    GTheme.Border_ModernChild   = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
    GTheme.Text_ModernChild     = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

    GTheme.ChildBg_Card = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    GTheme.Border_Card  = ImVec4(0.80f, 0.80f, 0.83f, 1.0f);
    GTheme.Text_Card    = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

    GTheme.TableRowBg_InfoTable     = ImVec4(0, 0, 0, 0);
    GTheme.TableRowBgAlt_InfoTable  = ImVec4(0, 0, 0, 0.03f);

    GTheme.Text_ModernTabBar               = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    GTheme.Tab_ModernTabBar                = ImVec4(0, 0, 0, 0);
    GTheme.TabHovered_ModernTabBar         = ImVec4(0.80f, 0.80f, 0.85f, 1.0f);
    GTheme.TabActive_ModernTabBar          = ImVec4(0.70f, 0.70f, 0.75f, 1.0f);
    GTheme.TabUnfocused_ModernTabBar       = ImVec4(0, 0, 0, 0);
    GTheme.TabUnfocusedActive_ModernTabBar = ImVec4(0.12f, 0.45f, 0.90f, 0.4f);

    GTheme.Button_ModernButton              = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);
    GTheme.ButtonHovered_ModernButton       = ImVec4(0.05f, 0.55f, 0.95f, 1.00f);
    GTheme.ButtonActive_ModernButton        = ImVec4(0.00f, 0.40f, 0.75f, 1.00f);
    GTheme.Text_ModernButton                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    GTheme.Text_SecondaryButton             = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    GTheme.Button_SecondaryButton           = ImVec4(0.94f, 0.94f, 0.96f, 0.00f);
    GTheme.ButtonHovered_SecondaryButton    = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    GTheme.ButtonActive_SecondaryButton     = ImVec4(0.85f, 0.85f, 0.88f, 1.00f);
    GTheme.Border_SecondaryButton           = ImVec4(0.75f, 0.75f, 0.80f, 1.00f);

    GTheme.Text_ModernCheckbox              = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    GTheme.FrameBg_ModernCheckbox           = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
    GTheme.FrameBgHovered_ModernCheckbox    = ImVec4(0.75f, 0.75f, 0.80f, 1.0f);
    GTheme.FrameBgActive_ModernCheckbox     = ImVec4(0.12f, 0.45f, 0.90f, 0.4f);
    GTheme.CheckMark_ModernCheckbox         = ImVec4(0.12f, 0.45f, 0.90f, 1.0f);

    GTheme.FrameBg_ModernInputTextMultiline          = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    GTheme.FrameBgHovered_ModernInputTextMultiline   = ImVec4(0.95f, 0.95f, 0.97f, 1.0f);
    GTheme.FrameBgActive_ModernInputTextMultiline    = ImVec4(0.90f, 0.90f, 0.93f, 1.0f);
    GTheme.Border_ModernInputTextMultiline           = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
    GTheme.TextSelectedBg_ModernInputTextMultiline   = ImVec4(0.12f, 0.45f, 0.90f, 0.25f);

    GTheme.Header_ModernSelectable          = ImVec4(0.90f, 0.92f, 0.96f, 1.00f);
    GTheme.HeaderHovered_ModernSelectable   = ImVec4(0.85f, 0.88f, 0.94f, 1.00f);
    GTheme.HeaderActive_ModernSelectable    = ImVec4(0.80f, 0.84f, 0.90f, 1.00f);
    GTheme.Text_Selected_ModernSelectable   = ImVec4(0.05f, 0.10f, 0.25f, 1.00f);
    GTheme.Text_UnSelected_ModernSelectable = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    GTheme.Text_ModernInputTextMultiline    = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

    GTheme.TableHeaderBg_ListTable  = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
    GTheme.Text_ListTable           = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    GTheme.TableRowBgAlt_ListTable  = ImVec4(0, 0, 0, 0.02f);
    GTheme.HeaderActive_ListTable   = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    GTheme.HeaderHovered_ListTable  = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);

    GTheme.Header_ModernCollapsingHeader        = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
    GTheme.HeaderHovered_ModernCollapsingHeader = ImVec4(0.85f, 0.85f, 0.88f, 1.00f);
    GTheme.HeaderActive_ModernCollapsingHeader  = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
    GTheme.Text_ModernCollapsingHeader          = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    
    GTheme.FrameBg_NormalCombo        = ImVec4(0.94f, 0.94f, 0.96f, 1.00f); 
    GTheme.FrameBgHovered_NormalCombo = ImVec4(0.88f, 0.88f, 0.92f, 1.00f); 
    GTheme.Border_NormalCombo         = ImVec4(0.80f, 0.80f, 0.83f, 1.00f); 
    GTheme.Text_NormalCombo           = ImVec4(0.15f, 0.15f, 0.18f, 1.00f); 
    GTheme.TextLaBel_NormalCombo      = ImVec4(0.15f, 0.15f, 0.18f, 1.00f); 
    GTheme.PopupBg_NormalCombo        = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);

    GTheme.Header_ModernTreeNode        = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    GTheme.HeaderHovered_ModernTreeNode = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    GTheme.HeaderActive_ModernTreeNode  = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    GTheme.Text_ModernTreeNode          = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
}
static inline void SetMidnightTheme() {
    // --- Modern Window Style (Nền xanh đen sâu, chữ trắng xanh nhạt) ---
    GTheme.Text_ModernWindowStyle           = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
    GTheme.WindowBg_ModernWindowStyle       = ImVec4(0.07f, 0.08f, 0.11f, 0.98f);
    GTheme.TitleBg_ModernWindowStyle        = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    GTheme.TitleBgActive_ModernWindowStyle  = ImVec4(0.09f, 0.11f, 0.15f, 1.00f);
    GTheme.Border_ModernWindowStyle         = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    GTheme.Separator_ModernWindowStyle      = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);

    // --- Modern Child (Phân cấp nhẹ hơn nền chính) ---
    GTheme.ChildBg_ModernChild  = ImVec4(0.09f, 0.11f, 0.16f, 1.0f);
    GTheme.Border_ModernChild   = ImVec4(0.18f, 0.22f, 0.30f, 1.0f);
    GTheme.Text_ModernChild     = ImVec4(0.92f, 0.95f, 0.98f, 1.0f);

    // --- Cards (Nổi bật trên nền Child) ---
    GTheme.ChildBg_Card = ImVec4(0.14f, 0.17f, 0.25f, 1.0f);
    GTheme.Border_Card  = ImVec4(0.22f, 0.28f, 0.40f, 1.0f);
    GTheme.Text_Card    = ImVec4(0.90f, 0.93f, 0.96f, 1.0f);

    // --- Info Table (Hàng xen kẽ) ---
    GTheme.TableRowBg_InfoTable     = ImVec4(0, 0, 0, 0);
    GTheme.TableRowBgAlt_InfoTable  = ImVec4(1, 1, 1, 0.03f);

    // --- Modern Tab Bar (Tông màu Indigo) ---
    GTheme.Text_ModernTabBar                = ImVec4(0.55f, 0.60f, 0.70f, 1.0f);
    GTheme.Tab_ModernTabBar                 = ImVec4(0, 0, 0, 0);
    GTheme.TabHovered_ModernTabBar          = ImVec4(0.20f, 0.25f, 0.40f, 1.0f);
    GTheme.TabActive_ModernTabBar           = ImVec4(0.15f, 0.18f, 0.30f, 1.0f);
    GTheme.TabUnfocused_ModernTabBar        = ImVec4(0, 0, 0, 0);
    GTheme.TabUnfocusedActive_ModernTabBar  = ImVec4(0.25f, 0.45f, 0.90f, 0.6f);

    // --- Modern Button (Màu xanh nước biển đậm - Primary) ---
    GTheme.Button_ModernButton              = ImVec4(0.20f, 0.40f, 0.75f, 1.00f);
    GTheme.ButtonHovered_ModernButton       = ImVec4(0.25f, 0.50f, 0.90f, 1.0f);
    GTheme.ButtonActive_ModernButton        = ImVec4(0.15f, 0.35f, 0.65f, 1.0f);
    GTheme.Text_ModernButton                = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);

    // --- Secondary Button (Trong suốt có viền) ---
    GTheme.Text_SecondaryButton             = ImVec4(0.85f, 0.90f, 1.00f, 1.00f);
    GTheme.Button_SecondaryButton           = ImVec4(0.15f, 0.20f, 0.30f, 0.00f);
    GTheme.ButtonHovered_SecondaryButton    = ImVec4(0.18f, 0.25f, 0.40f, 0.70f);
    GTheme.ButtonActive_SecondaryButton     = ImVec4(0.14f, 0.18f, 0.25f, 1.00f);
    GTheme.Border_SecondaryButton           = ImVec4(0.25f, 0.35f, 0.50f, 1.0f);

    // --- Modern Checkbox ---
    GTheme.Text_ModernCheckbox              = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
    GTheme.FrameBg_ModernCheckbox           = ImVec4(0.15f, 0.18f, 0.25f, 1.0f);
    GTheme.FrameBgHovered_ModernCheckbox    = ImVec4(0.20f, 0.25f, 0.35f, 1.00f);
    GTheme.FrameBgActive_ModernCheckbox     = ImVec4(0.25f, 0.45f, 0.90f, 0.5f);
    GTheme.CheckMark_ModernCheckbox         = ImVec4(0.30f, 0.55f, 1.00f, 1.0f);

    // --- Modern Input Text ---
    GTheme.FrameBg_ModernInputTextMultiline         = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
    GTheme.FrameBgHovered_ModernInputTextMultiline  = ImVec4(0.12f, 0.15f, 0.20f, 1.00f);
    GTheme.FrameBgActive_ModernInputTextMultiline   = ImVec4(0.06f, 0.08f, 0.12f, 1.00f);
    GTheme.Border_ModernInputTextMultiline          = ImVec4(0.20f, 0.25f, 0.35f, 1.00f);
    GTheme.TextSelectedBg_ModernInputTextMultiline  = ImVec4(0.20f, 0.45f, 0.90f, 0.45f);
    GTheme.Text_ModernInputTextMultiline            = ImVec4(0.90f, 0.93f, 0.96f, 1.0f);

    // --- Modern Selectable (Vùng chọn) ---
    GTheme.Header_ModernSelectable          = ImVec4(0.18f, 0.25f, 0.40f, 1.00f);
    GTheme.HeaderHovered_ModernSelectable   = ImVec4(0.22f, 0.30f, 0.50f, 1.00f);
    GTheme.HeaderActive_ModernSelectable    = ImVec4(0.28f, 0.38f, 0.60f, 1.00f);
    GTheme.Text_Selected_ModernSelectable   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    GTheme.Text_UnSelected_ModernSelectable = ImVec4(0.65f, 0.70f, 0.80f, 1.00f);

    // --- List Table ---
    GTheme.TableHeaderBg_ListTable  = ImVec4(0.09f, 0.11f, 0.16f, 1.0f);
    GTheme.Text_ListTable           = ImVec4(0.55f, 0.60f, 0.70f, 1.0f);
    GTheme.TableRowBgAlt_ListTable  = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
    GTheme.HeaderActive_ListTable   = ImVec4(0.18f, 0.25f, 0.40f, 1.00f);
    GTheme.HeaderHovered_ListTable  = ImVec4(0.18f, 0.25f, 0.40f, 1.00f);
    
    // --- Modern Collapsing Header ---
    GTheme.Header_ModernCollapsingHeader        = ImVec4(0.12f, 0.15f, 0.22f, 1.0f);
    GTheme.HeaderHovered_ModernCollapsingHeader  = ImVec4(0.18f, 0.22f, 0.32f, 1.00f);
    GTheme.HeaderActive_ModernCollapsingHeader   = ImVec4(0.22f, 0.28f, 0.40f, 1.00f);
    GTheme.Text_ModernCollapsingHeader          = ImVec4(0.85f, 0.90f, 0.95f, 1.00f);

    // --- Combo Boxes ---
    GTheme.FrameBg_NormalCombo          = ImVec4(0.08f, 0.10f, 0.14f, 1.00f); 
    GTheme.FrameBgHovered_NormalCombo   = ImVec4(0.12f, 0.16f, 0.25f, 1.00f); 
    GTheme.Border_NormalCombo           = ImVec4(0.20f, 0.25f, 0.35f, 1.00f); 
    GTheme.Text_NormalCombo             = ImVec4(0.90f, 0.93f, 0.96f, 1.00f); 
    GTheme.TextLaBel_NormalCombo        = ImVec4(0.70f, 0.75f, 0.85f, 1.00f); 
    GTheme.PopupBg_NormalCombo          = ImVec4(0.09f, 0.11f, 0.16f, 0.98f);

    // --- Modern TreeNode ---
    GTheme.Header_ModernTreeNode        = ImVec4(0.16f, 0.22f, 0.35f, 1.00f);
    GTheme.HeaderHovered_ModernTreeNode = ImVec4(0.22f, 0.30f, 0.45f, 1.00f);
    GTheme.HeaderActive_ModernTreeNode  = ImVec4(0.10f, 0.12f, 0.18f, 1.00f);
    GTheme.Text_ModernTreeNode          = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
}
static inline void SetRetroTheme() {
    // --- Modern Window Style (Tông màu nâu đen sáp, chữ vàng cát) ---
    GTheme.Text_ModernWindowStyle           = ImVec4(0.92f, 0.86f, 0.70f, 1.00f); // Retro Cream
    GTheme.WindowBg_ModernWindowStyle       = ImVec4(0.16f, 0.16f, 0.16f, 1.00f); // Dark Charcoal
    GTheme.TitleBg_ModernWindowStyle        = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    GTheme.TitleBgActive_ModernWindowStyle  = ImVec4(0.15f, 0.14f, 0.13f, 1.00f);
    GTheme.Border_ModernWindowStyle         = ImVec4(0.25f, 0.24f, 0.23f, 1.00f);
    GTheme.Separator_ModernWindowStyle      = ImVec4(0.25f, 0.24f, 0.23f, 1.00f);

    // --- Modern Child (Màu tối hơn nền chính một chút) ---
    GTheme.ChildBg_ModernChild  = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
    GTheme.Border_ModernChild   = ImVec4(0.25f, 0.24f, 0.23f, 1.0f);
    GTheme.Text_ModernChild     = ImVec4(0.92f, 0.86f, 0.70f, 1.0f);

    // --- Cards (Màu nâu nhẹ, ấm) ---
    GTheme.ChildBg_Card = ImVec4(0.20f, 0.19f, 0.18f, 1.0f);
    GTheme.Border_Card  = ImVec4(0.31f, 0.29f, 0.27f, 1.0f);
    GTheme.Text_Card    = ImVec4(0.85f, 0.80f, 0.65f, 1.0f);

    // --- Info Table ---
    GTheme.TableRowBg_InfoTable     = ImVec4(0, 0, 0, 0);
    GTheme.TableRowBgAlt_InfoTable  = ImVec4(1.0f, 0.90f, 0.70f, 0.02f);

    // --- Modern Tab Bar (Tông màu Cam cháy / Rỉ sét) ---
    GTheme.Text_ModernTabBar                = ImVec4(0.65f, 0.60f, 0.50f, 1.0f);
    GTheme.Tab_ModernTabBar                 = ImVec4(0, 0, 0, 0);
    GTheme.TabHovered_ModernTabBar          = ImVec4(0.30f, 0.28f, 0.26f, 1.0f);
    GTheme.TabActive_ModernTabBar           = ImVec4(0.25f, 0.23f, 0.21f, 1.0f);
    GTheme.TabUnfocused_ModernTabBar        = ImVec4(0, 0, 0, 0);
    GTheme.TabUnfocusedActive_ModernTabBar  = ImVec4(0.84f, 0.60f, 0.13f, 0.5f);

    // --- Modern Button (Màu vàng mù tạt đặc trưng của Gruvbox) ---
    GTheme.Button_ModernButton              = ImVec4(0.84f, 0.60f, 0.13f, 1.00f); 
    GTheme.ButtonHovered_ModernButton       = ImVec4(0.98f, 0.74f, 0.18f, 1.0f);
    GTheme.ButtonActive_ModernButton        = ImVec4(0.72f, 0.51f, 0.10f, 1.0f);
    GTheme.Text_ModernButton                = ImVec4(0.11f, 0.11f, 0.11f, 1.0f); // Chữ tối trên nền sáng

    // --- Secondary Button (Trong suốt viền nâu) ---
    GTheme.Text_SecondaryButton             = ImVec4(0.84f, 0.60f, 0.13f, 1.00f);
    GTheme.Button_SecondaryButton           = ImVec4(0, 0, 0, 0);
    GTheme.ButtonHovered_SecondaryButton    = ImVec4(0.25f, 0.24f, 0.23f, 0.80f);
    GTheme.ButtonActive_SecondaryButton     = ImVec4(0.20f, 0.19f, 0.18f, 1.00f);
    GTheme.Border_SecondaryButton           = ImVec4(0.40f, 0.35f, 0.30f, 1.0f);

    // --- Modern Checkbox (Màu xanh rêu) ---
    GTheme.Text_ModernCheckbox              = ImVec4(0.92f, 0.86f, 0.70f, 1.00f);
    GTheme.FrameBg_ModernCheckbox           = ImVec4(0.20f, 0.19f, 0.18f, 1.0f);
    GTheme.FrameBgHovered_ModernCheckbox    = ImVec4(0.31f, 0.29f, 0.27f, 1.0f);
    GTheme.FrameBgActive_ModernCheckbox     = ImVec4(0.58f, 0.63f, 0.13f, 0.5f);
    GTheme.CheckMark_ModernCheckbox         = ImVec4(0.58f, 0.63f, 0.13f, 1.00f); // Moss Green

    // --- Modern Input Text ---
    GTheme.FrameBg_ModernInputTextMultiline         = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    GTheme.FrameBgHovered_ModernInputTextMultiline  = ImVec4(0.20f, 0.19f, 0.18f, 1.00f);
    GTheme.FrameBgActive_ModernInputTextMultiline   = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    GTheme.Border_ModernInputTextMultiline          = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    GTheme.TextSelectedBg_ModernInputTextMultiline  = ImVec4(0.40f, 0.35f, 0.30f, 0.50f);
    GTheme.Text_ModernInputTextMultiline            = ImVec4(0.92f, 0.86f, 0.70f, 1.0f);

    // --- Modern Selectable ---
    GTheme.Header_ModernSelectable          = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    GTheme.HeaderHovered_ModernSelectable   = ImVec4(0.40f, 0.36f, 0.33f, 1.00f);
    GTheme.HeaderActive_ModernSelectable    = ImVec4(0.25f, 0.24f, 0.23f, 1.00f);
    GTheme.Text_Selected_ModernSelectable   = ImVec4(0.98f, 0.74f, 0.18f, 1.00f);
    GTheme.Text_UnSelected_ModernSelectable = ImVec4(0.65f, 0.60f, 0.50f, 1.00f);

    // --- List Table ---
    GTheme.TableHeaderBg_ListTable  = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    GTheme.Text_ListTable           = ImVec4(0.60f, 0.55f, 0.45f, 1.0f);
    GTheme.TableRowBgAlt_ListTable  = ImVec4(1.0f, 0.90f, 0.70f, 0.02f);
    GTheme.HeaderActive_ListTable   = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    GTheme.HeaderHovered_ListTable  = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    
    // --- Modern Collapsing Header ---
    GTheme.Header_ModernCollapsingHeader        = ImVec4(0.20f, 0.19f, 0.18f, 1.0f);
    GTheme.HeaderHovered_ModernCollapsingHeader  = ImVec4(0.25f, 0.24f, 0.23f, 1.00f);
    GTheme.HeaderActive_ModernCollapsingHeader   = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    GTheme.Text_ModernCollapsingHeader          = ImVec4(0.92f, 0.86f, 0.70f, 1.00f);

    // --- Combo Boxes ---
    GTheme.FrameBg_NormalCombo          = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); 
    GTheme.FrameBgHovered_NormalCombo   = ImVec4(0.20f, 0.19f, 0.18f, 1.00f); 
    GTheme.Border_NormalCombo           = ImVec4(0.31f, 0.29f, 0.27f, 1.00f); 
    GTheme.Text_NormalCombo             = ImVec4(0.92f, 0.86f, 0.70f, 1.00f); 
    GTheme.TextLaBel_NormalCombo        = ImVec4(0.85f, 0.80f, 0.65f, 1.00f); 
    GTheme.PopupBg_NormalCombo          = ImVec4(0.16f, 0.16f, 0.16f, 0.98f);

    // --- Modern TreeNode ---
    GTheme.Header_ModernTreeNode        = ImVec4(0.25f, 0.24f, 0.23f, 1.00f);
    GTheme.HeaderHovered_ModernTreeNode = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    GTheme.HeaderActive_ModernTreeNode  = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    GTheme.Text_ModernTreeNode          = ImVec4(0.92f, 0.86f, 0.70f, 1.00f);
}
 
extern std::map<ThemeType, ThemeColors> ThemeLibrary;

inline void InitThemeLibrary() {

    // Theme Dark
    SetDarkTheme(); // Hàm cũ của bạn
    ThemeLibrary[ThemeType::DarkMode] = GTheme;

    // Theme Light
    SetLightTheme(); // Hàm cũ của bạn
    ThemeLibrary[ThemeType::LightMode] = GTheme;

    SetMidnightTheme();
    ThemeLibrary[ThemeType::MidnightMode] = GTheme;

    SetRetroTheme();
    ThemeLibrary[ThemeType::RetroMode] = GTheme;

    // Theme Nord (Ví dụ theme thứ 3)
    // SetNordTheme();
    // ThemeLibrary[ThemeType::Nord] = GTheme;
}
struct ThemeTransition {
    ThemeColors startTheme;   // Màu lúc bắt đầu bấm nút
    ThemeColors targetTheme;  // Màu đích muốn tới
    float progress = 1.0f;    // 1.0 nghĩa là đã xong, < 1.0 là đang chạy
    float speed = 2.5f;       // Tốc độ chuyển đổi
    bool active = false;
};
extern ThemeTransition GTrans;
inline void ApplyTheme(ThemeType type) {
    if (ThemeLibrary.find(type) == ThemeLibrary.end()) return;

    GTrans.startTheme = GTheme;              // Lưu trạng thái hiện tại làm điểm gốc
    GTrans.targetTheme = ThemeLibrary[type]; // Lấy theme đích từ thư viện
    GTrans.progress = 0.0f;                  // Reset tiến trình về 0
    GTrans.active = true;
}

inline void UpdateTheme(float deltaTime) {
    if (!GTrans.active) return;

    GTrans.progress += deltaTime * GTrans.speed;
    if (GTrans.progress >= 1.0f) {
        GTrans.progress = 1.0f;
        GTrans.active = false;
    }

    // Ép kiểu sang float* để duyệt toàn bộ struct (Nếu struct chỉ chứa ImVec4/float)
    float* current = (float*)&GTheme;
    float* start = (float*)&GTrans.startTheme;
    float* target = (float*)&GTrans.targetTheme;

    size_t numFloats = sizeof(ThemeColors) / sizeof(float);

    for (size_t i = 0; i < numFloats; i++) {
        // Công thức Lerp: Current = Start + (Target - Start) * Progress
        current[i] = start[i] + (target[i] - start[i]) * GTrans.progress;
    }
}

namespace CusTomImGui{
    // InfoRow an toàn, hỗ trợ std::string
    inline void InfoRow(const char* label, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        
        char buf[1024];
        int len = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        // Kiểm tra nếu giá trị rỗng hoặc chỉ có khoảng trắng
        bool isEmpty = (len <= 0 || buf[0] == '\0');

        ImGui::TableNextRow(ImGuiTableRowFlags_None, 24.0f);

        // Cột 1: Label
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", label);

        // Cột 2: Value
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();

        if (isEmpty) {
            // Nếu rỗng, hiện dấu gạch ngang mờ (N/A)
            ImGui::TextDisabled("None"); 
        } else {
            ImGui::TextUnformatted(buf);
        }

        // Chỉ cho phép Copy nếu có dữ liệu
        //if (!isEmpty && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        //    ImGui::SetClipboardText(buf);
        //}
    }

    inline bool BeginInfoTable(const char* id, int column_count = 2, float first_col_width = 120.0f, ImGuiTableFlags extra_flags = 0) {
        // Flags: Thêm NoBordersInBody để UI trông phẳng (Flat Design)
        ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | 
                                ImGuiTableFlags_RowBg | 
                                ImGuiTableFlags_NoSavedSettings | 
                                ImGuiTableFlags_NoBordersInBody | 
                                extra_flags;

        // Đẩy khoảng cách giữa các ô ra một chút (Padding)
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 4.0f));

        if (ImGui::BeginTable(id, column_count, flags)) {
            ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, first_col_width);
            for (int i = 1; i < column_count; i++) {
                ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);
            }

            ImGui::PushStyleColor(ImGuiCol_TableRowBg,    GTheme.TableRowBg_InfoTable);
            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, GTheme.TableRowBgAlt_InfoTable); 
            
            return true;
        }
        
        ImGui::PopStyleVar(); // Pop CellPadding nếu BeginTable fail
        return false;
    }

    inline void EndInfoTable() {
        ImGui::EndTable();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(); // Pop CellPadding
    }

    inline bool BeginCard() {
        // Sử dụng màu nền Card nhẹ nhàng, tiệp với tông Dark của Window
        ImGui::PushStyleColor(ImGuiCol_ChildBg, GTheme.ChildBg_Card); 
        ImGui::PushStyleColor(ImGuiCol_Border,  GTheme.Border_Card); // Viền mảnh
        ImGui::PushStyleColor(ImGuiCol_Text,    GTheme.Text_Card); // Chữ trắng sáng

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        // Dùng ID động để tránh trùng lặp nếu có nhiều Card
        if(ImGui::BeginChild(ImGui::GetID("##card_inner"), ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding)){
            return true;
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
        return false;
    }

    inline void EndCard() {
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);
    }

    inline bool BeginModernChild(const char* str_id, ImVec2 size = ImVec2(0, 0), bool border = false, ImGuiWindowFlags extra_flags = 0) {
        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Tùy chỉnh Style cho hiện đại
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f); // Bo góc mềm mại
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
        
        // Màu sắc (Sử dụng màu tối nhẹ hoặc trắng tinh khôi)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, GTheme.ChildBg_ModernChild); 
        ImGui::PushStyleColor(ImGuiCol_Border, GTheme.Border_ModernChild);
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text_ModernChild);

        if(ImGui::BeginChild(str_id, size, border, extra_flags | ImGuiWindowFlags_NoScrollbar)) return true;

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);

        return false;
    }

    inline void EndModernChild() {
        ImGui::EndChild();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }
    inline bool BeginModernTabBar(const char* id) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(15.0f, 0.0f)); 
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f)); // Tab cao hơn nhìn sang hơn
        
        // Màu sắc
        ImGui::PushStyleColor(ImGuiCol_Text,          GTheme.Text_ModernTabBar);  // Text mặc định hơi tối
        ImGui::PushStyleColor(ImGuiCol_Tab,           GTheme.Tab_ModernTabBar);              // Trong suốt khi ko chọn
        ImGui::PushStyleColor(ImGuiCol_TabHovered,    GTheme.TabHovered_ModernTabBar);
        ImGui::PushStyleColor(ImGuiCol_TabActive,     GTheme.TabActive_ModernTabBar); // Tiệp màu với ChildBg bên dưới
        ImGui::PushStyleColor(ImGuiCol_TabUnfocused,  GTheme.TabUnfocused_ModernTabBar);
        
        // Đường kẻ dưới Tab Active (Màu Accent)
        ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, GTheme.TabUnfocused_ModernTabBar);

        if(ImGui::BeginTabBar(id, ImGuiTabBarFlags_NoTabListScrollingButtons | ImGuiTabBarFlags_FittingPolicyResizeDown)) {
            return true;
        }

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);

        return false;
    }
    inline void EndModernTabBar() {
        ImGui::EndTabBar();
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);
    }
    inline void PushModernWindowStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        
        // 1. Bo góc và viền
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
        
        // 2. Tiêu đề (Title bar) - Làm cho nó cao hơn và phẳng hơn
        ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f)); // Căn giữa title
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));    // Tăng độ cao title bar
        
        // 3. Màu sắc hiện đại (Dark Theme tinh tế)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, GTheme.WindowBg_ModernWindowStyle);
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text_ModernWindowStyle);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, GTheme.TitleBg_ModernWindowStyle);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, GTheme.TitleBgActive_ModernWindowStyle);
        ImGui::PushStyleColor(ImGuiCol_Border, GTheme.Border_ModernWindowStyle);
        ImGui::PushStyleColor(ImGuiCol_Separator, GTheme.Separator_ModernWindowStyle);
    }
    inline void PopModernWindowStyle() {
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(5);
    }
    inline bool ModernButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
        // 1. Kiểm tra xem người dùng có truyền size cố định hay không
        bool has_custom_size = (size.x != 0.0f || size.y != 0.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        
        // 2. Nếu có size, reset Padding để ImGui tự căn giữa text trong không gian đó
        // Nếu không có size, dùng Padding mặc định để nút trông "dày dặn"
        if (has_custom_size) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 8));
        }
        
        // 3. Setup Màu sắc
        ImGui::PushStyleColor(ImGuiCol_Button,        GTheme.Button_ModernButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GTheme.ButtonHovered_ModernButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  GTheme.ButtonActive_ModernButton);
        ImGui::PushStyleColor(ImGuiCol_Text,           GTheme.Text_ModernButton);

        // 4. Gọi hàm Button gốc
        bool pressed = ImGui::Button(label, size);

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2); // Pop FrameRounding và FramePadding

        return pressed;
    }
    inline bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f); // Có viền nhẹ

        ImGui::PushStyleColor(ImGuiCol_Button,        GTheme.Button_SecondaryButton); // Trong suốt
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GTheme.ButtonHovered_SecondaryButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  GTheme.ButtonActive_SecondaryButton);
        ImGui::PushStyleColor(ImGuiCol_Border,        GTheme.Border_SecondaryButton);
        ImGui::PushStyleColor(ImGuiCol_Text,          GTheme.Text_SecondaryButton);

        bool pressed = ImGui::Button(label, size);

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);
        return pressed;
    }
    inline bool ModernCheckbox(const char* label, bool* v) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        //ImGui::PushStyleVar(ImGuiStyleVar_CheckMarkSize, 14.0f); // Dấu tích lớn dễ nhìn

        ImGui::PushStyleColor(ImGuiCol_Text,             GTheme.Text_ModernCheckbox);

        // Màu nền ô Check
        ImGui::PushStyleColor(ImGuiCol_FrameBg,          GTheme.FrameBg_ModernCheckbox);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   GTheme.FrameBgHovered_ModernCheckbox);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    GTheme.FrameBgActive_ModernCheckbox);
        
        // Màu dấu tích khi được chọn
        ImGui::PushStyleColor(ImGuiCol_CheckMark,        GTheme.CheckMark_ModernCheckbox);

        bool changed = ImGui::Checkbox(label, v);

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();
        return changed;
    }
    inline bool ModernInputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(-1, 0), ImGuiInputTextFlags flags = 0) {
        // 1. Bo góc và Padding cho nội dung bên trong ô nhập
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10)); // Tạo khoảng trống cho chữ "thở"
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        // 2. Màu sắc (Nền tối, Chữ trắng, Viền xanh khi Focus)
        ImGui::PushStyleColor(ImGuiCol_FrameBg,          GTheme.FrameBg_ModernInputTextMultiline); // Nền ô nhập
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   GTheme.FrameBgHovered_ModernInputTextMultiline);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    GTheme.FrameBgActive_ModernInputTextMultiline);
        
        // Màu viền (Rất quan trọng để nhận biết đang gõ)
        ImGui::PushStyleColor(ImGuiCol_Border,           GTheme.Border_ModernInputTextMultiline); 
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,   GTheme.TextSelectedBg_ModernInputTextMultiline); // Màu khi bôi đen chữ

        ImGui::PushStyleColor(ImGuiCol_Text,             GTheme.Text_ModernInputTextMultiline);

        bool changed = ImGui::InputTextMultiline(label, buf, buf_size, size, flags);

        // Kiểm tra nếu đang gõ thì đổi màu viền sang xanh Blue (Accent)
        if (ImGui::IsItemActive()) {
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 
                ImColor(40, 110, 230, 255), 6.0f, 0, 1.5f
            );
        }

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(3);
        return changed;
    }
    inline bool ModernSelectable(const char* label, bool selected, ImGuiSelectableFlags flags = 0, const ImVec2& size_arg = ImVec2(0, 0)) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        // 1. Tăng khoảng cách (Padding) cho Item
        // Giúp item cao hơn, dễ nhìn và dễ click hơn
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
        
        // 2. Bo góc cho phần highlight (nền khi chọn hoặc hover)
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        // 3. Màu sắc hiện đại
        ImGui::PushStyleColor(ImGuiCol_Header,        GTheme.Header_ModernSelectable); // Màu khi được chọn (Selected)
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GTheme.HeaderHovered_ModernSelectable); // Màu khi di chuột qua
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  GTheme.HeaderActive_ModernSelectable); // Màu khi nhấn giữ
        
        if (selected) ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text_Selected_ModernSelectable);
        else          ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text_UnSelected_ModernSelectable);

        // Gọi hàm gốc của ImGui
        // Sử dụng size.y lớn hơn một chút để tạo danh sách thoáng đãng
        ImVec2 size = size_arg;
        if (size.y == 0.0f) size.y = ImGui::GetTextLineHeightWithSpacing() + 4.0f;

        bool pressed = ImGui::Selectable(label, selected, flags, size);

        // 4. Hoàn trả Style
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);

        return pressed;
    }
    struct TableCol {
        const char* name;
        float width; // 0.0f là Stretch, > 0.0f là Fixed
    };
    inline bool BeginListTable(const char* id, const std::vector<TableCol>& cols, ImGuiTableFlags extra_flags = 0) {
        ImGuiTableFlags flags = ImGuiTableFlags_RowBg | 
                                ImGuiTableFlags_SizingFixedFit | 
                                ImGuiTableFlags_NoSavedSettings | 
                                ImGuiTableFlags_BordersInnerV | 
                                extra_flags;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10, 8));

        if (ImGui::BeginTable(id, (int)cols.size(), flags)) {
            // Setup từng cột dựa trên vector truyền vào
            for (const auto& col : cols) {
                ImGuiTableColumnFlags c_flags = (col.width > 0.0f) ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch;
                ImGui::TableSetupColumn(col.name, c_flags, col.width);
            }

            // Style cho Header
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,   GTheme.TableHeaderBg_ListTable);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,    GTheme.HeaderActive_ListTable);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   GTheme.HeaderHovered_ListTable);
            ImGui::PushStyleColor(ImGuiCol_Text,            GTheme.Text_ListTable);
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor(4);

            // Màu xen kẽ hàng
            //ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, GTheme.TableRowBgAlt_ListTable);
            return true;
        }
        
        ImGui::PopStyleVar(); // Pop CellPadding nếu fail
        return false;
    }
    inline void EndListTable() {
        ImGui::EndTable();
        //ImGui::PopStyleColor(); // Pop TableRowBgAlt
        ImGui::PopStyleVar();   // Pop CellPadding
    }
    inline bool BeginListRow(float height = 28.0f) {
        ImGui::TableNextRow(ImGuiTableRowFlags_None, height);
        
        // Tạo một ID ẩn cho hàng để bắt sự kiện click trên toàn bộ hàng
        ImGui::TableNextColumn(); 
        ImGui::PushID(ImGui::GetCursorPosY());
        
        // Trả về true nếu người dùng click vào hàng này (sẽ kiểm tra ở cuối hàng)
        return true; 
    }

    inline void EndListRow() {
        ImGui::PopID();
    }

    // Kiểm tra xem hàng vừa vẽ có được click hay không
    inline bool IsRowClicked() {
        return ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped) && ImGui::IsMouseReleased(0);
    }

    inline bool ModernCollapsingHeader(const char* id, ImGuiTreeNodeFlags flags = 0) {
        // Bo góc nhẹ cho header nếu muốn đồng bộ với Combo
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        
        // Push bảng màu từ GTheme
        ImGui::PushStyleColor(ImGuiCol_Header,        GTheme.Header_ModernCollapsingHeader);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GTheme.HeaderHovered_ModernCollapsingHeader);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  GTheme.HeaderActive_ModernCollapsingHeader);
        ImGui::PushStyleColor(ImGuiCol_Text,          GTheme.Text_ModernCollapsingHeader); // Dùng chung màu text cho đồng bộ

        bool res = ImGui::CollapsingHeader(id, flags);

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);

        return res;
    }
    // --- HELPER 2: MODERN SEARCH COMBO (Nâng cao: Search + Max Height + Scroll) ---
    inline bool ModernSearchCombo(const char* label,
         std::string& current_value,
        const std::vector<std::string>& options,
        float custom_width = 200.0f,
        int max_items_visible = 6,
        std::function<bool(std::string&)> on_validate_confirm = nullptr,
        std::function<std::string(const std::string&)> on_get_dynamic_opt = nullptr)
        {

        bool value_confirmed = false;
        ImGuiID popup_id = ImGui::GetID(label);

        // 1. Static/Persistent state để lưu giá trị đang gõ (chưa xác nhận)
        // Sử dụng ID của widget để tránh xung đột giữa các combo khác nhau
        static ImGuiID active_id = 0;
        static char search_buf[128] = "";

        // 2. Style cho Label
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.TextLaBel_NormalCombo);
        ImGui::TextDisabled("%s", label);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(custom_width);

        // 3. Style cho ô InputText
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_FrameBg,         GTheme.FrameBg_NormalCombo);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  GTheme.FrameBgHovered_NormalCombo);
        ImGui::PushStyleColor(ImGuiCol_Border,          GTheme.Border_NormalCombo);
        ImGui::PushStyleColor(ImGuiCol_Text,            GTheme.Text_NormalCombo);

        // Nếu ô input này vừa được kích hoạt, copy giá trị hiện tại vào buffer tạm
        if (ImGui::IsItemDeactivated()) {
            active_id = 0; 
        }

        // Hiển thị buffer tạm nếu đang focus, ngược lại hiển thị giá trị thật
        char* display_buf = (active_id == popup_id) ? search_buf : (char*)current_value.c_str();
        std::string input_to_validate = search_buf;
        // Thêm flag ImGuiInputTextFlags_EnterReturnsTrue để biết khi nào nhấn Enter
        if (ImGui::InputTextEx("##search_input", "Type to search...", display_buf, 128, ImVec2(custom_width, 0), ImGuiInputTextFlags_EnterReturnsTrue)) {
            // Nhấn Enter: Xác nhận giá trị trong buffer (nếu muốn cho phép nhập text tự do)
            // Hoặc có thể để trống nếu bạn CHỈ muốn cho phép chọn từ danh sách
            
            if (on_validate_confirm && on_validate_confirm(input_to_validate)) {
                current_value = input_to_validate;
                value_confirmed = true;
                
            }
            active_id = 0;
            ImGui::CloseCurrentPopup();
            
        }
        if (ImGui::IsItemDeactivated()) {
            // Khi người dùng click ra ngoài hoặc tab đi chỗ khác
            // Nếu không phải là do vừa nhấn Enter (đã đóng popup), ta reset buffer
            active_id = 0; 
        }

        if (ImGui::IsItemActivated()) {
            active_id = popup_id;
            snprintf(search_buf, sizeof(search_buf), "%s", current_value.c_str());
            ImGui::OpenPopup(popup_id);
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);

        // 4. Popup Danh sách
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 2));
        ImGui::SetNextWindowSizeConstraints(ImVec2(custom_width, 0), ImVec2(custom_width, FLT_MAX));

        ImGui::PushStyleColor(ImGuiCol_PopupBg, GTheme.PopupBg_NormalCombo);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));

    if (ImGui::BeginPopupEx(popup_id, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_ChildWindow)) {
        
        std::string filter = search_buf;
        std::string filter_lower = filter;
        std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);
        
        std::vector<std::string> filtered_options;

        // 1. Lấy Dynamic Option từ Callback (Ví dụ: gõ "15" -> hiện "15px")
        std::string dynamic_val = "";
        if (on_get_dynamic_opt) {
            dynamic_val = on_get_dynamic_opt(filter); // Truyền filter thô để callback xử lý
            if (!dynamic_val.empty()) {
                filtered_options.push_back(dynamic_val);
            }
        }

        // 2. Lọc danh sách gốc và tránh trùng với Dynamic Option
        for (const auto& opt : options) {
            // Kiểm tra xem có khớp với filter không
            std::string o_lower = opt;
            std::transform(o_lower.begin(), o_lower.end(), o_lower.begin(), ::tolower);
            
            bool matches = filter_lower.empty() || o_lower.find(filter_lower) != std::string::npos;
            
            // Tránh trùng: Nếu option gốc giống hệt dynamic_val thì không add thêm nữa
            bool is_duplicate = (!dynamic_val.empty() && opt == dynamic_val);

            if (matches && !is_duplicate) {
                filtered_options.push_back(opt);
            }
        }

        // --- Tính toán chiều cao Popup ---
        float row_height = ImGui::GetTextLineHeightWithSpacing() + 12.0f;
        float display_count = (float)std::min((int)filtered_options.size(), max_items_visible);
        // Nếu không có kết quả, hiện 1 dòng để báo "No results"
        float child_height = (filtered_options.empty()) ? row_height : (display_count * row_height);

        if (ImGui::BeginChild("##combo_scroll", ImVec2(0, child_height), false, ImGuiWindowFlags_NoScrollbar)) {
            if (filtered_options.empty()) {
                ImGui::Indent(10);
                ImGui::TextDisabled("No results found");
                ImGui::Unindent(10);
            } else {
                for (const auto& opt : filtered_options) {
                    bool is_selected = (current_value == opt);
                    
                    // Truncate text nếu quá dài
                    std::string truncated_opt = TextUtils::TruncateTextByPixels(opt.c_str(), custom_width - 25.0f);

                    if (CusTomImGui::ModernSelectable(truncated_opt.c_str(), is_selected)) {
                        // Sử dụng callback validate để đảm bảo chuẩn hóa dữ liệu trước khi lưu
                        std::string validated_val = opt;
                        if (on_validate_confirm && on_validate_confirm(validated_val)) {
                            current_value = validated_val;
                        } else {
                            current_value = opt; // Fallback
                        }
                        
                        value_confirmed = true;
                        active_id = 0; // Reset trạng thái gõ
                        ImGui::CloseCurrentPopup();
                    }
                    
                    if (ImGui::IsItemHovered() && truncated_opt != opt) {
                        ImGui::SetTooltip("%s", opt.c_str());
                    }
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        return value_confirmed;
    }

    inline bool NormalCombo(const char* label, std::string& current_item, const std::vector<std::string>& options, float custom_width = 200.0f, int max_items_visible = 5) {
        bool changed = false;

        // 1. Style cho Label
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.TextLaBel_NormalCombo);
        ImGui::TextDisabled("%s", label);
        ImGui::PopStyleColor();

        // 2. Cấu hình độ rộng cho Combo tiếp theo
        // Sử dụng custom_width để giới hạn chiều rộng hiển thị
        ImGui::SetNextItemWidth(custom_width);

        // 3. Push Style Vars
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        // 4. Push Style Colors
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GTheme.FrameBg_NormalCombo);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, GTheme.FrameBgHovered_NormalCombo);
        ImGui::PushStyleColor(ImGuiCol_Border, GTheme.Border_NormalCombo);
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text_NormalCombo);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, GTheme.PopupBg_NormalCombo);

        // Tính toán chiều cao tối đa cho danh sách thả xuống
        float item_height = ImGui::GetTextLineHeightWithSpacing() + 8.0f;
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(custom_width, item_height * max_items_visible));

        // --- Xử lý cắt tỉa chữ (Truncate) cho Item đang chọn ---
        // Trừ đi một khoảng padding và mũi tên của combo (thường khoảng 30-40px)
        float available_text_width = custom_width - 35.0f; 
        std::string truncated_display = TextUtils::TruncateTextByPixels(current_item.c_str(), available_text_width);

        // ID "##" để ẩn nhãn mặc định, dùng truncated_display để hiển thị
        if (ImGui::BeginCombo("##normal_combo", truncated_display.c_str(), ImGuiComboFlags_HeightLarge)) {
            for (const auto& opt : options) {
                bool is_selected = (current_item == opt);

                // Cắt tỉa chữ cho từng option trong danh sách thả xuống nếu cần
                std::string opt_display = TextUtils::TruncateTextByPixels(opt.c_str(), custom_width - 20.0f);

                if (CusTomImGui::ModernSelectable(opt_display.c_str(), is_selected)) {
                    current_item = opt; // Lưu giá trị gốc, không lưu chuỗi đã cắt
                    changed = true;
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Dọn dẹp Stack (5 Colors, 3 Vars)
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);

        return changed;
    }
    // --- HELPER: MODERN POPUP (Dùng cho Modal/Dialog) ---
    inline bool BeginModernPopup(const char* name, bool* open = NULL, ImGuiWindowFlags flags = 0) {
        // Áp dụng Style Window hiện đại cho Popup
        CusTomImGui::PushModernWindowStyle();
        
        // Thêm hiệu ứng làm mờ nền (Dim background)
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.6f));

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        bool isOpen = ImGui::BeginPopupModal(name, open, flags | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
        
        if (!isOpen) {
            ImGui::PopStyleColor();
            CusTomImGui::PopModernWindowStyle();
        }
        return isOpen;
    }

    inline void EndModernPopup() {
        ImGui::EndPopup();
        ImGui::PopStyleColor();
        CusTomImGui::PopModernWindowStyle();
    }

    inline void EndModernTreeNode(){
        ImGui::TreePop();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }
    inline bool ModernTreeNode(const char* id) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 6)); 
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

        ImGui::PushStyleColor(ImGuiCol_Header,         GTheme.Header_ModernTreeNode); 
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  GTheme.HeaderHovered_ModernTreeNode); 
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,   GTheme.HeaderActive_ModernTreeNode); 
        ImGui::PushStyleColor(ImGuiCol_Text,           GTheme.Text_ModernTreeNode); 

        bool open = ImGui::TreeNode(id);
        
        if (!open) {
            // Nếu không mở, chúng ta phải Pop ngay tại đây
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
        }
        // Nếu mở, việc Pop sẽ do EndModernTreeNode đảm nhận
        return open;
    }
    inline bool ModernSliderFloat(const char* label, float* v, float v_min, float v_max, 
                                float height = 4.0f, float grab_radius = 8.0f, 
                                const char* format = "%.3f", float custom_width = -1.0f)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;

        // --- 1. Label ---
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text_ModernChild);
        ImGui::TextDisabled("%s: %s", label, TextUtils::Format(format, *v).c_str());
        ImGui::PopStyleColor();

        // --- 2. Tính vùng tương tác ---
        const float w = (custom_width > 0) ? custom_width : ImGui::CalcItemWidth();
        float interaction_height = grab_radius * 2.5f;
        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size(w, interaction_height);

        ImGui::InvisibleButton(label, size);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked();

        ImGuiID id = window->GetID(label);
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;

        bool changed = false;

        // --- 3. Cập nhật giá trị khi kéo ---
        if (active) {
            float t = (mouse_pos.x - pos.x) / size.x;
            t = ImClamp(t, 0.0f, 1.0f);
            float new_v = v_min + t * (v_max - v_min);
            if (new_v != *v) {
                *v = new_v;
                changed = true;
            }
        }

        // --- 4. Vẽ track ---
        ImDrawList* draw_list = window->DrawList;
        float center_y = pos.y + size.y * 0.5f;
        ImVec2 track_p1(pos.x, center_y - height * 0.5f);
        ImVec2 track_p2(pos.x + size.x, center_y + height * 0.5f);

        // Track nền
        draw_list->AddRectFilled(track_p1, track_p2, ToCol32(GTheme.FrameBg_NormalCombo), height * 0.5f);

        // Track active
        float t_fill = (*v - v_min) / (v_max - v_min);
        t_fill = ImClamp(t_fill, 0.0f, 1.0f);
        ImVec2 active_p2(track_p1.x + t_fill * size.x, track_p2.y);
        draw_list->AddRectFilled(track_p1, active_p2, ToCol32(GTheme.CheckMark_ModernCheckbox), height * 0.5f);

        // --- 5. Border track ---
        draw_list->AddRect(track_p1, track_p2, ToCol32(GTheme.Border_ModernChild), height * 0.5f, 0, 1.0f);

        // --- 6. Vẽ Grab ---
        ImVec2 grab_center(track_p1.x + t_fill * size.x, center_y);
        float visual_radius = grab_radius;
        if (active) visual_radius *= 1.2f;
        else if (hovered) visual_radius *= 1.1f;
        
        draw_list->AddCircleFilled(grab_center, visual_radius + 1.0f, IM_COL32(0,0,0,40));
        draw_list->AddCircleFilled(grab_center, visual_radius, ToCol32(GTheme.Text_Selected_ModernSelectable));
        draw_list->AddCircle(grab_center, visual_radius, ToCol32(GTheme.CheckMark_ModernCheckbox), 0, 2.0f);

        return changed;
    }
    inline bool ModernInputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0) {
        // 1. Setup Style tương tự Multiline nhưng padding dọc nhỏ hơn để cân đối
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8)); 
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        // 2. Màu sắc từ Theme
        ImGui::PushStyleColor(ImGuiCol_FrameBg,          GTheme.FrameBg_ModernInputTextMultiline); 
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   GTheme.FrameBgHovered_ModernInputTextMultiline);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    GTheme.FrameBgActive_ModernInputTextMultiline);
        ImGui::PushStyleColor(ImGuiCol_Border,           GTheme.Border_ModernInputTextMultiline); 
        ImGui::PushStyleColor(ImGuiCol_Text,             GTheme.Text_ModernInputTextMultiline);

        bool changed = ImGui::InputText(label, buf, buf_size, flags);

        // 3. Hiệu ứng viền Accent khi active
        if (ImGui::IsItemActive()) {
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 
                ImColor(40, 110, 230, 255), 6.0f, 0, 1.5f
            );
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);
        return changed;
    }
    inline bool ModernSmallButton(const char* label) {
        // Nút nhỏ cần bo góc ít hơn một chút hoặc giữ nguyên để đồng bộ
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2)); // Padding cực nhỏ cho Small Button
        
        // Sử dụng màu của SecondaryButton hoặc một màu Neutral hơn
        ImGui::PushStyleColor(ImGuiCol_Button,          GTheme.Button_SecondaryButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,   GTheme.ButtonHovered_ModernButton); // Hover vẫn cho màu chính
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,    GTheme.ButtonActive_ModernButton);
        ImGui::PushStyleColor(ImGuiCol_Text,            GTheme.Text_SecondaryButton);

        bool pressed = ImGui::Button(label);

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        return pressed;
    }
    inline bool ModernArrowButton(const char* str_id, ImGuiDir dir, ImVec2 size = ImVec2(0, 0)) {
        // 1. Setup Style
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        
        bool custom_size = (size.x != 0.0f || size.y != 0.0f);
        if (!custom_size) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        }

        // 2. Setup Colors
        ImGui::PushStyleColor(ImGuiCol_Button,        GTheme.Button_SecondaryButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GTheme.ButtonHovered_ModernButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  GTheme.ButtonActive_ModernButton);
        ImGui::PushStyleColor(ImGuiCol_Text,           GTheme.Text_SecondaryButton);

        // 3. Render Button
        // Sử dụng Button với label rỗng để lấy hitbox
        bool pressed = ImGui::Button(str_id, size);

        // 4. Vẽ mũi tên lên trên Button vừa tạo
        if (ImGui::IsItemVisible()) {
            // Thay thế LastItemRect bằng các hàm an toàn hơn:
            ImVec2 pos_min = ImGui::GetItemRectMin();
            ImVec2 pos_max = ImGui::GetItemRectMax();
            
            float arrow_size = ImGui::GetFontSize();
            // Tính toán tâm của nút
            ImVec2 center = ImVec2(pos_min.x + (pos_max.x - pos_min.x) * 0.5f, 
                                pos_min.y + (pos_max.y - pos_min.y) * 0.5f);
            
            // Vẽ mũi tên căn giữa
            ImGui::RenderArrow(ImGui::GetWindowDrawList(), 
                            ImVec2(center.x - arrow_size * 0.45f, center.y - arrow_size * 0.45f), 
                            ImGui::GetColorU32(ImGuiCol_Text), 
                            dir);
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);
        
        return pressed;
    }


}
