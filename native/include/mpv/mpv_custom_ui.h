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
    // --- 1. Window & Layout (Nền tảng chính) ---
    ImVec4 WindowBg;            // Nền cửa sổ chính
    ImVec4 ChildBg;             // Nền các vùng con (Child Window)
    ImVec4 PopupBg;             // Nền Tooltip, Combo, Context Menu
    ImVec4 Border;              // Màu viền (Borders)
    ImVec4 Separator;           // Đường kẻ phân cách

    // --- 2. Title Bar (Thanh tiêu đề) ---
    ImVec4 TitleBg;             // Nền tiêu đề khi không hoạt động
    ImVec4 TitleBgActive;       // Nền tiêu đề khi đang tập trung (Focus)

    // --- 3. Typography (Chữ & Lựa chọn) ---
    ImVec4 Text;                // Chữ chính (Main text)
    ImVec4 TextDisabled;        // Chữ bị vô hiệu hóa, label phụ, placeholder
    ImVec4 TextSelected;        // Màu chữ khi được bôi đen (Highlight)
    ImVec4 TextSelectedBg;      // Màu nền khi chữ được bôi đen

    // --- 4. Widgets & Frames (Input, Checkbox, Card) ---
    ImVec4 FrameBg;             // Nền mặc định cho Input, Box, Panel
    ImVec4 FrameBgHovered;      // Khi di chuột qua Input/Frame
    ImVec4 FrameBgActive;       // Khi đang tương tác/nhập liệu
    ImVec4 CheckMark;           // Màu dấu tích Checkbox, Radio button

    // --- 5. Buttons (Nút bấm) ---
    ImVec4 Button;              // Màu nút mặc định
    ImVec4 ButtonHovered;       // Khi di chuột qua nút
    ImVec4 ButtonActive;        // Khi nhấn giữ nút

    // --- 6. Headers & Trees (Selectable, TreeNode) ---
    ImVec4 Header;              // Màu khi item được chọn
    ImVec4 HeaderHovered;       // Khi di chuột qua item
    ImVec4 HeaderActive;        // Khi nhấn giữ item

    // --- 7. Tabs (Thanh tab) ---
    ImVec4 Tab;                 // Nền tab mặc định
    ImVec4 TabHovered;          // Khi di chuột qua tab
    ImVec4 TabActive;           // Tab đang hiển thị
    ImVec4 TabUnfocused;        // Tab khi cửa sổ không được focus
    ImVec4 TabUnfocusedActive;  // Tab đang hoạt động nhưng cửa sổ không focus

    // --- 8. Tables (Bảng dữ liệu) ---
    ImVec4 TableHeaderBg;       // Nền dòng tiêu đề bảng
    ImVec4 TableRowBg;          // Nền dòng lẻ (hoặc nền mặc định dòng)
    ImVec4 TableRowBgAlt;       // Nền dòng chẵn (Zebra striping)
};
extern ThemeColors GTheme;
extern ThemeColors GTheme;
static inline void SetDarkTheme() {
    // --- Nền & Viền ---
    GTheme.WindowBg          = ImVec4(0.10f, 0.10f, 0.12f, 0.95f);
    GTheme.FrameBg           = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    GTheme.FrameBgHovered    = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
    GTheme.FrameBgActive     = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    GTheme.PopupBg           = ImVec4(0.08f, 0.08f, 0.09f, 0.98f);
    GTheme.ChildBg           = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    GTheme.Border            = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    GTheme.Separator         = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);

    // --- Chữ & Title ---
    GTheme.Text              = ImVec4(1.00f, 1.00f, 1.00f, 0.95f);
    GTheme.TextDisabled      = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    GTheme.TextSelected      = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    GTheme.TextSelectedBg    = ImVec4(0.12f, 0.45f, 0.90f, 0.35f);
    GTheme.TitleBg           = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    GTheme.TitleBgActive     = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);

    // --- Tương tác (Buttons / Checkbox) ---
    GTheme.Button            = ImVec4(0.12f, 0.45f, 0.90f, 1.00f);
    GTheme.ButtonHovered     = ImVec4(0.15f, 0.55f, 1.00f, 1.00f);
    GTheme.ButtonActive      = ImVec4(0.10f, 0.35f, 0.80f, 1.00f);
    GTheme.CheckMark         = ImVec4(0.12f, 0.45f, 0.90f, 1.00f);

    // --- Headers & Tabs ---
    GTheme.Header            = ImVec4(0.18f, 0.21f, 0.25f, 1.00f);
    GTheme.HeaderHovered     = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    GTheme.HeaderActive      = ImVec4(0.28f, 0.33f, 0.40f, 1.00f);
    GTheme.Tab               = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    GTheme.TabHovered        = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
    GTheme.TabActive         = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    GTheme.TabUnfocused      = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    GTheme.TabUnfocusedActive = ImVec4(0.12f, 0.45f, 0.90f, 0.40f);

    // --- Tables ---
    GTheme.TableRowBg        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TableRowBgAlt     = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    GTheme.TableHeaderBg     = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
}
static inline void SetLightTheme() {
    GTheme.WindowBg          = ImVec4(0.94f, 0.94f, 0.96f, 1.00f);
    GTheme.FrameBg           = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    GTheme.FrameBgHovered    = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    GTheme.FrameBgActive     = ImVec4(0.90f, 0.90f, 0.93f, 1.00f);
    GTheme.PopupBg           = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    GTheme.ChildBg           = ImVec4(0.97f, 0.97f, 0.98f, 1.00f);
    GTheme.Border            = ImVec4(0.75f, 0.75f, 0.80f, 1.00f);
    GTheme.Separator         = ImVec4(0.75f, 0.75f, 0.78f, 1.00f);

    GTheme.Text              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    GTheme.TextDisabled      = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    GTheme.TextSelected      = ImVec4(0.00f, 0.10f, 0.25f, 1.00f);
    GTheme.TextSelectedBg    = ImVec4(0.00f, 0.47f, 0.83f, 0.25f);
    GTheme.TitleBg           = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    GTheme.TitleBgActive     = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);

    GTheme.Button            = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);
    GTheme.ButtonHovered     = ImVec4(0.05f, 0.55f, 0.95f, 1.00f);
    GTheme.ButtonActive      = ImVec4(0.00f, 0.40f, 0.75f, 1.00f);
    GTheme.CheckMark         = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);

    GTheme.Header            = ImVec4(0.90f, 0.92f, 0.96f, 1.00f);
    GTheme.HeaderHovered     = ImVec4(0.85f, 0.88f, 0.94f, 1.00f);
    GTheme.HeaderActive      = ImVec4(0.80f, 0.84f, 0.90f, 1.00f);
    GTheme.Tab               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TabHovered        = ImVec4(0.80f, 0.80f, 0.85f, 1.00f);
    GTheme.TabActive         = ImVec4(0.70f, 0.70f, 0.75f, 1.00f);
    GTheme.TabUnfocused      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TabUnfocusedActive = ImVec4(0.00f, 0.47f, 0.83f, 0.40f);

    GTheme.TableRowBg        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TableRowBgAlt     = ImVec4(0.00f, 0.00f, 0.00f, 0.03f);
    GTheme.TableHeaderBg     = ImVec4(0.85f, 0.85f, 0.88f, 1.00f);
}
static inline void SetMidnightTheme() {
    GTheme.WindowBg          = ImVec4(0.07f, 0.08f, 0.11f, 0.98f);
    GTheme.FrameBg           = ImVec4(0.09f, 0.11f, 0.16f, 1.00f);
    GTheme.FrameBgHovered    = ImVec4(0.14f, 0.17f, 0.25f, 1.00f);
    GTheme.FrameBgActive     = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
    GTheme.PopupBg           = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    GTheme.ChildBg           = ImVec4(0.09f, 0.11f, 0.16f, 1.00f);
    GTheme.Border            = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    GTheme.Separator         = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);

    GTheme.Text              = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
    GTheme.TextDisabled      = ImVec4(0.40f, 0.45f, 0.55f, 1.00f);
    GTheme.TextSelected      = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    GTheme.TextSelectedBg    = ImVec4(0.25f, 0.45f, 0.90f, 0.45f);
    GTheme.TitleBg           = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    GTheme.TitleBgActive     = ImVec4(0.09f, 0.11f, 0.15f, 1.00f);

    GTheme.Button            = ImVec4(0.20f, 0.40f, 0.75f, 1.00f);
    GTheme.ButtonHovered     = ImVec4(0.25f, 0.50f, 0.90f, 1.00f);
    GTheme.ButtonActive      = ImVec4(0.15f, 0.35f, 0.65f, 1.00f);
    GTheme.CheckMark         = ImVec4(0.30f, 0.55f, 1.00f, 1.00f);

    GTheme.Header            = ImVec4(0.18f, 0.25f, 0.40f, 1.00f);
    GTheme.HeaderHovered     = ImVec4(0.22f, 0.30f, 0.50f, 1.00f);
    GTheme.HeaderActive      = ImVec4(0.28f, 0.38f, 0.60f, 1.00f);
    GTheme.Tab               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TabHovered        = ImVec4(0.20f, 0.25f, 0.40f, 1.00f);
    GTheme.TabActive         = ImVec4(0.15f, 0.18f, 0.30f, 1.00f);
    GTheme.TabUnfocused      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TabUnfocusedActive = ImVec4(0.25f, 0.45f, 0.90f, 0.60f);

    GTheme.TableRowBg        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TableRowBgAlt     = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    GTheme.TableHeaderBg     = ImVec4(0.12f, 0.15f, 0.22f, 1.00f);
}
static inline void SetRetroTheme() {
    GTheme.WindowBg          = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    GTheme.FrameBg           = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    GTheme.FrameBgHovered    = ImVec4(0.20f, 0.19f, 0.18f, 1.00f);
    GTheme.FrameBgActive     = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    GTheme.PopupBg           = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    GTheme.ChildBg           = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    GTheme.Border            = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    GTheme.Separator         = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);

    GTheme.Text              = ImVec4(0.92f, 0.86f, 0.70f, 1.00f);
    GTheme.TextDisabled      = ImVec4(0.50f, 0.45f, 0.40f, 1.00f);
    GTheme.TextSelected      = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    GTheme.TextSelectedBg    = ImVec4(0.84f, 0.60f, 0.13f, 0.35f);
    GTheme.TitleBg           = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    GTheme.TitleBgActive     = ImVec4(0.15f, 0.14f, 0.13f, 1.00f);

    GTheme.Button            = ImVec4(0.84f, 0.60f, 0.13f, 1.00f);
    GTheme.ButtonHovered     = ImVec4(0.98f, 0.74f, 0.18f, 1.00f);
    GTheme.ButtonActive      = ImVec4(0.72f, 0.51f, 0.10f, 1.00f);
    GTheme.CheckMark         = ImVec4(0.58f, 0.63f, 0.13f, 1.00f);

    GTheme.Header            = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    GTheme.HeaderHovered     = ImVec4(0.40f, 0.36f, 0.33f, 1.00f);
    GTheme.HeaderActive      = ImVec4(0.25f, 0.24f, 0.23f, 1.00f);
    GTheme.Tab               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TabHovered        = ImVec4(0.30f, 0.28f, 0.26f, 1.00f);
    GTheme.TabActive         = ImVec4(0.25f, 0.23f, 0.21f, 1.00f);
    GTheme.TabUnfocused      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TabUnfocusedActive = ImVec4(0.84f, 0.60f, 0.13f, 0.50f);

    GTheme.TableRowBg        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    GTheme.TableRowBgAlt     = ImVec4(1.00f, 0.90f, 0.70f, 0.02f);
    GTheme.TableHeaderBg     = ImVec4(0.18f, 0.17f, 0.16f, 1.00f);
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

            ImGui::PushStyleColor(ImGuiCol_TableRowBg,    GTheme.TableRowBg);
            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, GTheme.TableRowBgAlt); 
            
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
        ImGui::PushStyleColor(ImGuiCol_ChildBg, GTheme.ChildBg); 
        ImGui::PushStyleColor(ImGuiCol_Border,  GTheme.Border); // Viền mảnh
        ImGui::PushStyleColor(ImGuiCol_Text,    GTheme.Text); // Chữ trắng sáng

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
        ImGui::PushStyleColor(ImGuiCol_ChildBg, GTheme.ChildBg); 
        ImGui::PushStyleColor(ImGuiCol_Border, GTheme.Border);
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text);

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
        ImGui::PushStyleColor(ImGuiCol_Text,          GTheme.Text);  // Text mặc định hơi tối
        ImGui::PushStyleColor(ImGuiCol_Tab,           GTheme.Tab);              // Trong suốt khi ko chọn
        ImGui::PushStyleColor(ImGuiCol_TabHovered,    GTheme.TabHovered);
        ImGui::PushStyleColor(ImGuiCol_TabActive,     GTheme.TabActive); // Tiệp màu với ChildBg bên dưới
        ImGui::PushStyleColor(ImGuiCol_TabUnfocused,  GTheme.TabUnfocused);
        
        // Đường kẻ dưới Tab Active (Màu Accent)
        ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, GTheme.TabUnfocusedActive);

        if(ImGui::BeginTabBar(id, ImGuiTabBarFlags_NoTabListScrollingButtons | ImGuiTabBarFlags_FittingPolicyResizeDown)) {
            return true;
        }

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);

        return false;
    }
    inline bool ModernTabItem(const char* label, bool* p_open = NULL, ImGuiTabItemFlags flags = 0) {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGuiID id = window->GetID(label);

        // 1. Tùy chỉnh màu sắc Tab
        ImGui::PushStyleColor(ImGuiCol_Tab,                ImVec4(0,0,0,0)); // Tab ko chọn thì trong suốt
        ImGui::PushStyleColor(ImGuiCol_TabHovered,         GTheme.TabHovered);
        ImGui::PushStyleColor(ImGuiCol_TabActive,          ImVec4(0,0,0,0)); // Active cũng ko nền để hiện thanh bar
        ImGui::PushStyleColor(ImGuiCol_TabUnfocused,       ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0,0,0,0));

        bool selected = ImGui::BeginTabItem(label, p_open, flags);

        // 2. Logic Animation cho thanh Bar bên dưới
        // Lưu ý: ImGui quản lý Tab hơi khác, ta cần dùng trạng thái đã chọn của frame trước
        float* pT = ImGui::GetStateStorage()->GetFloatRef(id, 0.0f);
        float target = ImGui::IsItemHovered() || selected ? 1.0f : 0.0f;
        *pT += (target - *pT) * g.IO.DeltaTime * 12.0f;

        // 3. Vẽ thanh chỉ báo (Indicator Bar)
        if (*pT > 0.01f) {
            ImVec2 p_min = ImGui::GetItemRectMin();
            ImVec2 p_max = ImGui::GetItemRectMax();
            float bar_width = (p_max.x - p_min.x) * 0.8f; // Thanh bar rộng 80% tab
            float offset = (p_max.x - p_min.x - bar_width) * 0.5f;

            ImVec2 b_min = ImVec2(p_min.x + offset, p_max.y - 2.0f);
            ImVec2 b_max = ImVec2(p_min.x + offset + (bar_width * *pT), p_max.y); // Dài dần ra

            ImVec4 col = selected ? GTheme.CheckMark : GTheme.TextDisabled;
            col.w *= *pT;

            window->DrawList->AddRectFilled(b_min, b_max, ImGui::GetColorU32(col), 10.0f);
        }
        if(!selected){
            ImGui::PopStyleColor(5);
            return selected;
        }

        return selected;
    }
    inline void EndModernTabItem() {
        ImGui::EndTabItem();
        ImGui::PopStyleColor(5);
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
        ImGui::PushStyleColor(ImGuiCol_WindowBg, GTheme.WindowBg);
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, GTheme.TitleBg);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, GTheme.TitleBgActive);
        ImGui::PushStyleColor(ImGuiCol_Border, GTheme.Border);
        ImGui::PushStyleColor(ImGuiCol_Separator, GTheme.Separator);
    }
    inline void PopModernWindowStyle() {
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(5);
    }
    inline bool ModernButton(const char* label, const ImVec2& size_arg = ImVec2(0, 0), bool primary = true) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID id = window->GetID(label);
        
        // Tính toán kích thước
        ImVec2 padding = ImVec2(15, 8);
        ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
        ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + padding.x * 2.0f, label_size.y + padding.y * 2.0f);
        
        const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
        ImGui::ItemSize(size);
        if (!ImGui::ItemAdd(bb, id)) return false;

        // Tương tác
        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        // Animation logic
        float frameTime = g.IO.DeltaTime * 10.0f;
        float* pAnim = window->StateStorage.GetFloatRef(id, 0.0f);
        *pAnim = ImClamp(*pAnim + (hovered ? frameTime : -frameTime), 0.0f, 1.0f);

        // Hiệu ứng "Phóng to" nhẹ khi hover (Scale)
        float scale = 1.0f + (*pAnim * 0.02f); 
        ImVec2 center = bb.GetCenter();
        
        // Màu sắc
        ImVec4 col_base = primary ? GTheme.Button : ImVec4(0,0,0,0);
        ImVec4 col_hover = GTheme.ButtonHovered;
        ImVec4 final_bg = ImLerp(col_base, col_hover, *pAnim);
        if (held) final_bg = GTheme.ButtonActive;

        // Vẽ nền (Sử dụng DrawList để bo góc mượt)
        window->DrawList->AddRectFilled(bb.Min - ImVec2(2 * *pAnim, 1 * *pAnim), 
                                        bb.Max + ImVec2(2 * *pAnim, 1 * *pAnim), 
                                        ImGui::ColorConvertFloat4ToU32(final_bg), 6.0f);

        // Nếu là Secondary, vẽ thêm viền
        if (!primary) {
            window->DrawList->AddRect(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(GTheme.Border), 6.0f);
        }

        // Vẽ chữ căn giữa
        ImGui::RenderTextClipped(bb.Min, bb.Max, label, NULL, &label_size, ImVec2(0.5f, 0.5f));

        return pressed;
    }

    // Secondary chỉ là gọi ModernButton với tham số false
    inline bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
        return ModernButton(label, size, false);
    }
    // Thêm biến static để lưu ID đang được hover toàn cục
    static ImGuiID G_LastHoveredID = 0;

    inline bool ModernButtonEx(const char* label, const ImVec2& size_arg = ImVec2(0, 0)) {

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID id = window->GetID(label);
        
        // Tính toán kích thước
        ImVec2 padding = ImVec2(15, 8);
        ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
        ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + padding.x * 2.0f, label_size.y + padding.y * 2.0f);
        
        const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
        ImGui::ItemSize(size);
        if (!ImGui::ItemAdd(bb, id)) return false;
        
        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        
        if (hovered) G_LastHoveredID = id;

        float frameTime = g.IO.DeltaTime * 10.0f;
        float* pAnim = window->StateStorage.GetFloatRef(id, 0.0f);
        *pAnim = ImClamp(*pAnim + (hovered ? frameTime : -frameTime), 0.0f, 1.0f);

        // Tính toán "Influence" (Sức ảnh hưởng)
        // Nếu nút này không phải nút đang hover trực tiếp, nhưng là nút lân cận
        float influence = 0.0f;
        if (!hovered && G_LastHoveredID != 0) {
            // Có thể tính khoảng cách giữa các ID hoặc vị trí, 
            // ở đây đơn giản là dùng một biến trạng thái lân cận
            influence = 0.2f; // Sáng nhẹ 20%
        }
        
        // Màu nền mix thêm influence
        ImVec4 final_bg = ImLerp(GTheme.Button, GTheme.ButtonHovered, *pAnim + influence);
        // ... vẽ tiếp ...
    }

    enum class CheckboxStyle {
        Circle,     // Kiểu chấm tròn (như Radio Button nhưng đa chọn)
        Tick,       // Kiểu dấu tích cổ điển (Vẽ bằng đường thẳng)
        Square      // Kiểu hình vuông đặc (Fill)
    };
    inline bool ModernCheckbox(const char* label, bool* v, CheckboxStyle style = CheckboxStyle::Tick) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& imStyle = g.Style;
        const ImGuiID id = window->GetID(label);

        // 1. Tính toán kích thước
        float square_sz = ImGui::GetFrameHeight(); // Kích thước chuẩn của checkbox
        ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
        
        ImVec2 pos = window->DC.CursorPos;
        // Tổng vùng tương tác bao gồm cả Box và Label
        const ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0 ? imStyle.ItemInnerSpacing.x + label_size.x : 0), square_sz));
        
        ImGui::ItemSize(total_bb, imStyle.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id)) return false;

        // 2. Tương tác
        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
        if (pressed) *v = !*v;

        // 3. Animation tCheck
        float* pT = window->StateStorage.GetFloatRef(id, *v ? 1.0f : 0.0f);
        *pT = ImClamp(*pT + (*v ? g.IO.DeltaTime * 12.0f : -g.IO.DeltaTime * 12.0f), 0.0f, 1.0f);

        // 4. Vẽ Box nền
        // Bo góc hoàn toàn nếu là Circle, bo nhẹ nếu là kiểu khác
        float rounding = (style == CheckboxStyle::Circle) ? square_sz * 0.5f : 4.0f;
        ImU32 col_bg = ImGui::GetColorU32((held && hovered) ? GTheme.FrameBgActive : hovered ? GTheme.FrameBgHovered : GTheme.FrameBg);
        
        window->DrawList->AddRectFilled(pos, pos + ImVec2(square_sz, square_sz), col_bg, rounding);
        
        // Vẽ viền mỏng để trông sắc nét hơn
        window->DrawList->AddRect(pos, pos + ImVec2(square_sz, square_sz), ImGui::GetColorU32(ToCol32(GTheme.Border), 0.5f), rounding);

        // 5. Vẽ nội dung Check (Dựa trên enum Style)
        if (*pT > 0.01f) {
            ImVec2 center = pos + ImVec2(square_sz * 0.5f, square_sz * 0.5f);
            ImU32 check_col = ImGui::GetColorU32(GTheme.CheckMark);
            // Hiệu ứng Fade Alpha theo animation
            check_col = (check_col & 0x00FFFFFF) | ((uint32_t)(*pT * 255) << 24);

            if (style == CheckboxStyle::Circle) {
                window->DrawList->AddCircleFilled(center, (square_sz * 0.25f) * *pT, check_col);
            }
            else if (style == CheckboxStyle::Square) {
                float pad = square_sz * 0.25f * (1.0f - *pT + 1.0f); // Nở ra từ tâm
                window->DrawList->AddRectFilled(pos + ImVec2(pad, pad), pos + ImVec2(square_sz - pad, square_sz - pad), check_col, 2.0f);
            }
            else if (style == CheckboxStyle::Tick) {
                // Vẽ dấu tích bằng đường thẳng (Polyline)
                float thickness = 2.0f;
                float size = square_sz * 0.6f * *pT;
                float x0 = center.x - size * 0.5f, y0 = center.y;
                float x1 = center.x - size * 0.1f, y1 = center.y + size * 0.4f;
                float x2 = center.x + size * 0.5f, y2 = center.y - size * 0.4f;
                
                window->DrawList->PathLineTo(ImVec2(x0, y0));
                window->DrawList->PathLineTo(ImVec2(x1, y1));
                window->DrawList->PathLineTo(ImVec2(x2, y2));
                window->DrawList->PathStroke(check_col, 0, thickness);
            }
        }

        // 6. CĂN GIỮA LABEL THEO CHIỀU DỌC (FIX)
        if (label_size.x > 0.0f) {
            // Tính toán offset Y để text nằm chính giữa Box theo pixel-perfect
            // font_size là chiều cao text, square_sz là chiều cao Box
            float text_y_offset = (square_sz - g.FontSize) * 0.5f;
            ImVec2 text_pos = ImVec2(pos.x + square_sz + imStyle.ItemInnerSpacing.x, pos.y + text_y_offset);
            
            ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text);
            ImGui::RenderText(text_pos, label);
            ImGui::PopStyleColor();
        }

        return pressed;
    }
    // Callback hỗ trợ riêng cho std::string
    static int StringResizeCallback(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            std::string* str = (std::string*)data->UserData;

            // Resize string theo yêu cầu của ImGui
            str->resize(data->BufTextLen);

            // Cập nhật lại buffer pointer
            data->Buf = str->data();

            // Cập nhật size buffer
            data->BufSize = (int)str->capacity() + 1;
        }
        return 0;
    }

    // Hàm bổ trợ để vẽ phần Decor (viền, animation)
    inline void RenderModernInputEffect(ImGuiID id, float* pFocusAnim) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        float target = ImGui::IsItemActive() ? 1.0f : 0.0f;
        *pFocusAnim += (target - *pFocusAnim) * ImGui::GetIO().DeltaTime * 10.0f;

        if (*pFocusAnim > 0.01f) {
            ImRect rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            rect.Expand(0.5f);
            ImVec4 accent_col = GTheme.CheckMark;
            accent_col.w *= *pFocusAnim;
            window->DrawList->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(accent_col), 6.0f, 0, 1.5f);
        }
    }
    inline bool ModernInputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(-1, 0), ImGuiInputTextFlags flags = 0) {
        ImGuiID id = ImGui::GetCurrentWindow()->GetID(label);
        
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_FrameBg,          GTheme.FrameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   GTheme.FrameBgHovered);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    GTheme.FrameBgActive);
        ImGui::PushStyleColor(ImGuiCol_Border,           GTheme.Border); 
        ImGui::PushStyleColor(ImGuiCol_Text,             GTheme.Text);

        bool changed = ImGui::InputTextMultiline(label, buf, buf_size, size, flags);

        float* pFocusAnim = ImGui::GetStateStorage()->GetFloatRef(id + 1, 0.0f);
        RenderModernInputEffect(id, pFocusAnim);

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);
        return changed;
    }
    inline bool ModernInputTextMultiline(const char* label, std::string& str, const ImVec2& size = ImVec2(-1, 0), ImGuiInputTextFlags flags = 0) {
        ImGuiID id = ImGui::GetCurrentWindow()->GetID(label);
        
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_FrameBg,          GTheme.FrameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   GTheme.FrameBgHovered);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    GTheme.FrameBgActive);
        ImGui::PushStyleColor(ImGuiCol_Border,           GTheme.Border); 
        ImGui::PushStyleColor(ImGuiCol_Text,             GTheme.Text);

        // Ép thêm flag Resize và dùng Callback
        flags |= ImGuiInputTextFlags_CallbackResize;
        bool changed = ImGui::InputTextMultiline(label, (char*)str.c_str(), str.capacity() + 1, size, flags, StringResizeCallback, (void*)&str);

        float* pFocusAnim = ImGui::GetStateStorage()->GetFloatRef(id + 1, 0.0f);
        RenderModernInputEffect(id, pFocusAnim);

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);
        return changed;
    }
    inline bool ModernInputText(const char* label, char* buf, size_t buf_size, float width, ImGuiInputTextFlags flags = 0) {
        ImGuiID id = ImGui::GetID(label);
        
        // Logic Animation & Glow
        float* pAnim = ImGui::GetStateStorage()->GetFloatRef(id + 500, 0.0f);
        bool is_active = ImGui::GetActiveID() == id;
        *pAnim += ((is_active ? 1.0f : 0.0f) - *pAnim) * ImGui::GetIO().DeltaTime * 12.0f;

        // Style cho ô Input
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GTheme.FrameBg);
        ImGui::PushStyleColor(ImGuiCol_Border, is_active ? GTheme.CheckMark : GTheme.Border);

        ImGui::SetNextItemWidth(width);
        bool changed = ImGui::InputTextEx(label, "Type here...", buf, (int)buf_size, ImVec2(width, 0), flags);

        // Vẽ hiệu ứng Glow khi Active (Focus)
        if (*pAnim > 0.01f) {
            ImRect rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            rect.Expand(1.5f * (*pAnim));
            ImVec4 glow_col = GTheme.CheckMark;
            glow_col.w *= (*pAnim * 0.4f);
            ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(glow_col), 6.0f, 0, 1.5f);
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return changed;
    }
    inline bool ModernInputText(const char* label, std::string& buffer, float width, ImGuiInputTextFlags flags = 0) {
        ImGuiID id = ImGui::GetID(label);
        ImGuiStorage* store = ImGui::GetStateStorage();
        
        // Animation state
        float* pAnim = store->GetFloatRef(id + 500, 0.0f);
        bool is_active = ImGui::GetActiveID() == id;
        *pAnim += ((is_active ? 1.0f : 0.0f) - *pAnim) * ImGui::GetIO().DeltaTime * 12.0f;

        // Style
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GTheme.FrameBg);
        ImGui::PushStyleColor(ImGuiCol_Border, is_active ? GTheme.CheckMark : GTheme.Border);

        ImGui::SetNextItemWidth(width);
        
        // Gọi InputText với Callback Resize
        flags |= ImGuiInputTextFlags_CallbackResize;
        bool changed = ImGui::InputTextEx(label, "Type to search...", (char*)buffer.data(), (int)buffer.capacity() + 1, ImVec2(width, 0), flags, StringResizeCallback, (void*)&buffer);

        // Vẽ hiệu ứng Glow khi Active
        if (*pAnim > 0.01f) {
            ImRect rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            rect.Expand(1.5f * (*pAnim));
            ImVec4 glow_col = GTheme.CheckMark;
            glow_col.w *= (*pAnim * 0.4f);
            ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(glow_col), 6.0f, 0, 1.5f);
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        
        return changed;
    }
    
    inline bool ModernSelectable(const char* label, bool selected, ImGuiSelectableFlags flags = 0, const ImVec2& size_arg = ImVec2(0, 0), bool enabled = true) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        const ImGuiID id = window->GetID(label);
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;

        // 1. Tính toán kích thước và vị trí
        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImGui::CalcItemSize(size_arg, ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() + 8.0f);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

        // 2. Quản lý Animation qua Storage
        // tHover: 0.0f -> 1.0f (hiệu ứng di chuột)
        // tSelect: 0.0f -> 1.0f (hiệu ứng khi được chọn)
        float* pTHover = window->StateStorage.GetFloatRef(id, 0.0f);
        float* pTSelect = window->StateStorage.GetFloatRef(id + 1, 0.0f); // Dùng offset ID để tránh trùng

        // 3. Logic tương tác
        bool hovered, held ;
        bool pressed = false;
        if (enabled) {
            pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
        }

        // Cập nhật giá trị animation (Interpolation)
        float frameTime = g.IO.DeltaTime * 12.0f; // Tốc độ animation
        // Nếu bị tắt, ép animation về 0 một cách mượt mà hoặc lập tức
        if (enabled) {
            *pTHover = ImClamp(*pTHover + (hovered ? frameTime : -frameTime), 0.0f, 1.0f);
            *pTSelect = ImClamp(*pTSelect + (selected ? frameTime : -frameTime), 0.0f, 1.0f);
        } else {
            *pTHover = 0.0f; // Tắt hoàn toàn hiệu ứng hover
            *pTSelect = selected ? 1.0f : 0.0f; // Chỉ giữ màu của selected nếu cần, hoặc ép về 0
        }

        // 4. Vẽ Background (Custom Drawing)
        if (( *pTHover > 0.0f || *pTSelect > 0.0f) && enabled) {
            // Màu nền mix giữa Header và HeaderHovered dựa trên tHover
            if(hovered || selected){
                ImVec4 bgColor = GTheme.Header; 
                if (*pTHover > 0.0f) {
                    bgColor.x = ImLerp(GTheme.Header.x, GTheme.HeaderHovered.x, *pTHover);
                    bgColor.y = ImLerp(GTheme.Header.y, GTheme.HeaderHovered.y, *pTHover);
                    bgColor.z = ImLerp(GTheme.Header.z, GTheme.HeaderHovered.z, *pTHover);
                    bgColor.w = ImLerp(GTheme.Header.w, GTheme.HeaderHovered.w, *pTHover);
                }
                
                // Nếu đang được chọn, ưu tiên hiển thị màu HeaderActive hoặc giữ màu trộn
                if (selected) bgColor = GTheme.HeaderActive;

                window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(bgColor), 4.0f);
            }
        }

        // 5. Vẽ Thanh Chỉ Báo (Selection Indicator) ở cạnh trái
        if (enabled && *pTSelect > 0.0f) {
            float indicatorHeight = (bb.Max.y - bb.Min.y) * 0.6f; // Cao 60% item
            float centerY = (bb.Min.y + bb.Max.y) * 0.5f;
            
            ImVec2 p1(bb.Min.x + 2.0f, centerY - (indicatorHeight * 0.5f) * *pTSelect);
            ImVec2 p2(bb.Min.x + 4.5f, centerY + (indicatorHeight * 0.5f) * *pTSelect);
            
            window->DrawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(GTheme.Button), 10.0f);
        }


        // 6. Vẽ Văn Bản với hiệu ứng trượt (Slide)
        if (label[0] != '#' || (label[1] != '\0' && label[1] != '#')) {
            float textOffsetX = 10.0f + (*pTHover * 4.0f); // Trượt sang phải 4px khi hover
            ImVec4 textColor = selected ? GTheme.TextSelected : GTheme.Text;
            
            // Mix màu chữ mượt mà nếu cần (Tùy chọn)
            window->DrawList->AddText(ImVec2(pos.x + textOffsetX, pos.y + (size.y - g.FontSize) * 0.5f), 
                                    ImGui::ColorConvertFloat4ToU32(textColor), label);
        }

        // 7. Kết thúc item
        ImGui::ItemSize(size);
        ImGui::ItemAdd(bb, id);

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
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,   GTheme.TableHeaderBg);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,    GTheme.HeaderActive);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   GTheme.HeaderHovered);
            ImGui::PushStyleColor(ImGuiCol_Text,            GTheme.Text);
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
        ImGui::PushStyleColor(ImGuiCol_Header,        GTheme.Header);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GTheme.HeaderHovered);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  GTheme.HeaderActive);
        ImGui::PushStyleColor(ImGuiCol_Text,          GTheme.Text); // Dùng chung màu text cho đồng bộ

        bool res = ImGui::CollapsingHeader(id, flags);

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);

        return res;
    }
    // --- HELPER: Vẽ phần thân danh sách cho Combo (Normal & Search) ---
    inline bool DrawComboPopupBody(
        const char* label,
        std::string& current_value, 
        const std::vector<std::string>& display_options, 
        float custom_width = 200.0f, 
        int max_items_visible = 5,
        std::function<bool(std::string&)> on_validate_confirm = nullptr) 
    {
        bool changed = false;

        // Tính toán chiều cao
        float row_height = ImGui::GetTextLineHeightWithSpacing() + 12.0f; // Khớp với padding của ModernSelectable
        float display_count = (float)std::min((int)display_options.size(), max_items_visible);
        
        // Nếu danh sách trống (thường xảy ra ở Search Combo), cho độ cao 1 dòng để hiện "No results"
        float child_height = display_options.empty() ? row_height : (display_count * row_height);
        std::string child_id = "##scrl_" + std::string(label);
        if (ImGui::BeginChild(child_id.c_str(), ImVec2(0, child_height), false, ImGuiWindowFlags_NoScrollbar)) {
            if (display_options.empty()) {
                ImGui::Indent(10);
                ImGui::TextDisabled("No results found");
                ImGui::Unindent(10);
            } else {
                for (const auto& opt : display_options) {
                    bool is_selected = (current_value == opt);
                    
                    // Truncate text nếu quá dài
                    std::string truncated_opt = TextUtils::TruncateTextByPixels(opt.c_str(), custom_width - 25.0f);

                    if (CusTomImGui::ModernSelectable(truncated_opt.c_str(), is_selected)) {
                        // Chạy Validation Callback nếu có (dành cho SearchCombo)
                        std::string validated_val = opt;
                        if (on_validate_confirm && on_validate_confirm(validated_val)) {
                            current_value = validated_val;
                        } else {
                            current_value = opt; // Fallback bình thường
                        }
                        
                        changed = true;
                        ImGui::CloseCurrentPopup(); // Tự động đóng Popup khi chọn xong
                    }
                    
                    // Hiện Tooltip nếu text bị cắt
                    if (ImGui::IsItemHovered() && truncated_opt != opt) {
                        ImGui::SetTooltip("%s", opt.c_str());
                    }
                    
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndChild();

        return changed;
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
        ImGuiID popup_id = ImGui::GetID((std::string(label) + "_popup").c_str());
        ImGuiID anim_id = ImGui::GetID((std::string(label) + "_=searchanim").c_str());
        ImGuiID active_id_key = ImGui::GetID((std::string(label) + "_active").c_str());
        ImGuiID buffer_id_key = ImGui::GetID((std::string(label) + "_buffer").c_str());

        // 1. Static/Persistent state để lưu giá trị đang gõ (chưa xác nhận)
        // Sử dụng ID của widget để tránh xung đột giữa các combo khác nhau
        ImGuiStorage* store = ImGui::GetStateStorage();

        int* active_id = store->GetIntRef(active_id_key, 0);
        static std::unordered_map<ImGuiID, std::string> search_buffers;
        if (search_buffers.find(buffer_id_key) == search_buffers.end()) {
            search_buffers[buffer_id_key] = current_value; // Chỉ chạy 1 lần duy nhất
        }
        std::string& search_buf = search_buffers[buffer_id_key];

        // 2. Style cho Label
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text);
        ImGui::TextDisabled("%s", label);
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(custom_width);

        // 3. Style cho ô InputText
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_FrameBg,         GTheme.FrameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  GTheme.FrameBgHovered);
        ImGui::PushStyleColor(ImGuiCol_Border,          GTheme.Border);
        ImGui::PushStyleColor(ImGuiCol_Text,            GTheme.Text);

        // Hiển thị buffer tạm nếu đang focus, ngược lại hiển thị giá trị thật
        // Thêm flag ImGuiInputTextFlags_EnterReturnsTrue để biết khi nào nhấn Enter
        if (ModernInputText((std::string("##input_") + label).c_str(), search_buf, custom_width, ImGuiInputTextFlags_EnterReturnsTrue)) {
            // Nhấn Enter: Xác nhận giá trị trong buffer (nếu muốn cho phép nhập text tự do)
            // Hoặc có thể để trống nếu bạn CHỈ muốn cho phép chọn từ danh sách
            
            if (on_validate_confirm && on_validate_confirm(search_buf)) {
                current_value = search_buf;
                value_confirmed = true;
                
            }
            *active_id = 0;
            ImGui::CloseCurrentPopup();
            
        }

        float* pSearchAnim = ImGui::GetStateStorage()->GetFloatRef(anim_id, 0.0f);
        float search_target = (*active_id == (int)popup_id) ? 1.0f : 0.0f;
        *pSearchAnim += (search_target - *pSearchAnim) * ImGui::GetIO().DeltaTime * 12.0f;

        if (*pSearchAnim > 0.01f) {
            ImRect input_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            input_rect.Expand(1.0f * (*pSearchAnim)); // Viền nở ra nhẹ khi focus
            
            ImVec4 glow_col = GTheme.CheckMark; 
            glow_col.w *= (*pSearchAnim * 0.4f); // Độ mờ của hiệu ứng Glow
            
            // Vẽ viền ngoài cùng mượt mà
            ImGui::GetWindowDrawList()->AddRect(input_rect.Min, input_rect.Max, 
                ImGui::GetColorU32(glow_col), 6.0f, 0, 1.5f);
        }
        if (ImGui::IsItemDeactivated()) {
            if (!value_confirmed) {
                search_buf = current_value;
            }
            *active_id = 0;
        }

        if (ImGui::IsItemActivated()) {
            *active_id = (int)popup_id;
            search_buf = current_value;
            ImGui::OpenPopup(popup_id);
        }


        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);

        // 4. Popup Danh sách
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 2));
        ImGui::SetNextWindowSizeConstraints(ImVec2(custom_width, 0), ImVec2(custom_width, FLT_MAX));

        ImGui::PushStyleColor(ImGuiCol_PopupBg, GTheme.PopupBg);
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

        if (DrawComboPopupBody(label, current_value, filtered_options,  custom_width, max_items_visible, on_validate_confirm)) {
            value_confirmed = true;
            *active_id = 0; 
        }
 
        ImGui::EndPopup();
    }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        return value_confirmed;
    }

    inline bool NormalCombo(const char* label, std::string& current_item, const std::vector<std::string>& options, float custom_width = 200.0f, int max_items_visible = 5) {
        bool changed = false;
        ImGuiID popup_id = ImGui::GetID((std::string(label) + "_popup").c_str());
        ImGuiID anim_id = ImGui::GetID((std::string(label) + "_anim").c_str());
        ImGuiID arrow_id = ImGui::GetID((std::string(label) + "_arrow").c_str());

        bool is_open = ImGui::IsPopupOpen(popup_id,ImGuiPopupFlags_None);

        // 1. Style cho Label
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text);
        ImGui::TextDisabled("%s", label);
        ImGui::PopStyleColor();

        // 2. State cho Animation
        float* pAnim = ImGui::GetStateStorage()->GetFloatRef(anim_id , 0.0f);
        float* pArrowAnim = ImGui::GetStateStorage()->GetFloatRef(arrow_id , 0.0f);

        // 3. Khởi tạo kích thước và Style cho Frame
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
        
        float frame_height = ImGui::GetFrameHeight();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + custom_width, p_min.y + frame_height);

        // --- XÂY DỰNG HEADER (NÚT BẤM) ---
        // Sử dụng InvisibleButton để nhận tương tác chuột nhưng không bị ImGui vẽ đè
        ImGui::PushID(label);
        ImGui::InvisibleButton("##btn", ImVec2(custom_width, frame_height));
        ImGui::PopID();
        ImRect rect(p_min, p_max);
        ImVec2 mouse = ImGui::GetIO().MousePos;

        // Hover (kể cả khi bị popup block)
        bool is_hovered =
            rect.Contains(mouse);

        // Click (chuột trái)
        bool is_clicked =
            is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        if(is_clicked){
            if (is_open)
                ImGui::CloseCurrentPopup();
            else
                ImGui::OpenPopupEx(popup_id);
        }
        is_open = ImGui::IsPopupOpen(popup_id ,ImGuiPopupFlags_None);
   
        // Logic Animation
        float target_anim = (is_hovered || is_open) ? 1.0f : 0.0f;
        *pAnim += (target_anim - *pAnim) * ImGui::GetIO().DeltaTime * 10.0f;
        
        float target_arrow = is_open ? 1.0f : 0.0f;
        *pArrowAnim += (target_arrow - *pArrowAnim) * ImGui::GetIO().DeltaTime * 12.0f;

        // Tự vẽ Frame (Background & Border)
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImU32 bg_col = ImGui::GetColorU32(is_open ? GTheme.FrameBg : (is_hovered ? GTheme.FrameBgHovered : GTheme.FrameBg));
        
        draw_list->AddRectFilled(p_min, p_max, bg_col, 6.0f);
        draw_list->AddRect(p_min, p_max, ImGui::GetColorU32(GTheme.Border), 6.0f, 0, 1.0f);

        // Vẽ viền sáng Glow (Giống hệt SearchCombo)
        if (*pAnim > 0.01f) {
            ImVec4 accent = GTheme.CheckMark;
            accent.w *= (*pAnim * 0.4f);
            draw_list->AddRect(p_min, p_max, ImGui::GetColorU32(accent), 6.0f, 0, 1.5f);
        }

        // Vẽ Text hiển thị (Đã cắt tỉa)
        float available_text_width = custom_width - 35.0f; 
        std::string truncated_display = TextUtils::TruncateTextByPixels(current_item.c_str(), available_text_width);
        // Cộng thêm FramePadding để text nằm giữa
        draw_list->AddText(ImVec2(p_min.x + 10.0f, p_min.y + 8.0f), ImGui::GetColorU32(GTheme.Text), truncated_display.c_str());

        // Vẽ mũi tên Vector có khả năng xoay
        float arrow_size = 5.0f;
        ImVec2 arrow_center = ImVec2(p_max.x - 18.0f, p_min.y + frame_height * 0.5f);
        float angle = *pArrowAnim * IM_PI; 
        auto RotatePt = [](ImVec2 p, ImVec2 center, float ang) {
            float s = sin(ang), c = cos(ang);
            p.x -= center.x; p.y -= center.y;
            return ImVec2(p.x * c - p.y * s + center.x, p.x * s + p.y * c + center.y);
        };
        ImVec2 p1 = RotatePt(ImVec2(arrow_center.x - arrow_size, arrow_center.y - arrow_size * 0.4f), arrow_center, angle);
        ImVec2 p2 = RotatePt(ImVec2(arrow_center.x + arrow_size, arrow_center.y - arrow_size * 0.4f), arrow_center, angle);
        ImVec2 p3 = RotatePt(ImVec2(arrow_center.x, arrow_center.y + arrow_size * 0.6f), arrow_center, angle);
        draw_list->AddTriangleFilled(p1, p2, p3, ImGui::GetColorU32(is_open ? GTheme.CheckMark : GTheme.Text));

        ImGui::PopStyleVar(2); // Dọn dẹp Frame Vars

        // --- XÂY DỰNG POPUP DANH SÁCH (Dùng chung logic ModernSearchCombo) ---
        ImGui::SetNextWindowPos(ImVec2(p_min.x, p_max.y + 2));
        ImGui::SetNextWindowSizeConstraints(ImVec2(custom_width, 0), ImVec2(custom_width, FLT_MAX));

        ImGui::PushStyleColor(ImGuiCol_PopupBg, GTheme.PopupBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));

        if (ImGui::BeginPopupEx(popup_id, ImGuiWindowFlags_NoTitleBar | 
                                          ImGuiWindowFlags_NoMove | 
                                          ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_ChildWindow )) {
            // Gọi thẳng Helper truyền list options gốc vào
            if (DrawComboPopupBody(label, current_item, options,  custom_width, max_items_visible ,nullptr)) {
                changed = true;
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        return changed;
    }
    // --- HELPER: MODERN POPUP (Dùng cho Modal/Dialog) ---
    inline bool BeginModernPopup(const char* name, bool* open = NULL, ImGuiWindowFlags flags = 0) {
        
        ImGuiID id = ImGui::GetID(name);
        float* appear_anim = ImGui::GetStateStorage()->GetFloatRef(id + 10, 0.0f);
        
        // Nếu popup đang mở, tăng dần animation
        if (ImGui::IsPopupOpen(name))
            *appear_anim = ImMin(*appear_anim + ImGui::GetIO().DeltaTime * 6.0f, 1.0f);
        else
            *appear_anim = 0.0f;
        // Áp dụng Style Window hiện đại cho Popup
        CusTomImGui::PushModernWindowStyle();
        
        ImVec4 dim_col = ImVec4(0, 0, 0, 0.6f * (*appear_anim));
        // Thêm hiệu ứng làm mờ nền (Dim background)
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, dim_col);

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

        ImGui::PushStyleColor(ImGuiCol_Header,         GTheme.Header); 
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  GTheme.HeaderHovered); 
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,   GTheme.HeaderActive); 
        ImGui::PushStyleColor(ImGuiCol_Text,           GTheme.Text); 

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
        ImGui::PushStyleColor(ImGuiCol_Text, GTheme.Text);
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
        draw_list->AddRectFilled(track_p1, track_p2, ToCol32(GTheme.FrameBg), height * 0.5f);

        // Track active
        float t_fill = (*v - v_min) / (v_max - v_min);
        t_fill = ImClamp(t_fill, 0.0f, 1.0f);
        ImVec2 active_p2(track_p1.x + t_fill * size.x, track_p2.y);
        draw_list->AddRectFilled(track_p1, active_p2, ToCol32(GTheme.CheckMark), height * 0.5f);

        // --- 5. Border track ---
        draw_list->AddRect(track_p1, track_p2, ToCol32(GTheme.Border), height * 0.5f, 0, 1.0f);

        // --- 6. Vẽ Grab ---
        ImVec2 grab_center(track_p1.x + t_fill * size.x, center_y);
        float visual_radius = grab_radius;
        if (active) visual_radius *= 1.2f;
        else if (hovered) visual_radius *= 1.1f;
        
        draw_list->AddCircleFilled(grab_center, visual_radius + 1.0f, IM_COL32(0,0,0,40));
        draw_list->AddCircleFilled(grab_center, visual_radius, ToCol32(GTheme.TextSelected));
        draw_list->AddCircle(grab_center, visual_radius, ToCol32(GTheme.CheckMark), 0, 2.0f);

        return changed;
    }
    inline bool ModernInputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0) {
        // 1. Setup Style tương tự Multiline nhưng padding dọc nhỏ hơn để cân đối
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8)); 
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        // 2. Màu sắc từ Theme
        ImGui::PushStyleColor(ImGuiCol_FrameBg,          GTheme.FrameBg); 
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   GTheme.FrameBgHovered);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    GTheme.FrameBgActive);
        ImGui::PushStyleColor(ImGuiCol_Border,           GTheme.Border); 
        ImGui::PushStyleColor(ImGuiCol_Text,             GTheme.Text);

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
        ImGui::PushStyleColor(ImGuiCol_Button,          GTheme.Button);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,   GTheme.ButtonHovered); // Hover vẫn cho màu chính
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,    GTheme.ButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Text,            GTheme.Text);

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
        ImGui::PushStyleColor(ImGuiCol_Button,        GTheme.Button);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GTheme.ButtonHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  GTheme.ButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Text,           GTheme.Text);

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
    inline void ShowTooltipDelayed(const char* text, bool hovering, double delaySeconds, const char* id){
        // 1. Xử lý delay
        bool shouldShow = SetDelayHover(hovering, delaySeconds, id);

        // 2. Quản lý Alpha riêng biệt cho từng tooltip bằng Storage ID của ImGui
        // Điều này giúp nhiều tooltip không bị dùng chung 1 biến alpha tĩnh
        ImGuiID storage_id = ImGui::GetID(id);
        float* pAlpha = ImGui::GetStateStorage()->GetFloatRef(storage_id, 0.0f);

        // 3. Cập nhật hiệu ứng Fade (Tăng tốc độ fade ra để cảm giác nhạy hơn)
        float fadeSpeed = 12.0f; 
        UpdateHoverAnim(*pAlpha, shouldShow, fadeSpeed);

        // Ngắt sớm nếu alpha quá nhỏ để tiết kiệm hiệu năng
        if (*pAlpha <= 0.001f)
            return;

        // 4. Thiết lập Style trước khi Begin
        // Sử dụng ImGuiWindowFlags_AlwaysAutoResize để giữ kích thước ổn định
        ImGui::PushStyleColor(ImGuiCol_PopupBg, GTheme.PopupBg); // Màu nền Tooltip
        ImGui::PushStyleColor(ImGuiCol_Border,  GTheme.Border); // Màu viền
        ImGui::PushStyleColor(ImGuiCol_Text,    GTheme.Text); // Màu chữ

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, *pAlpha);                      // Giữ hiệu ứng fade in/out
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));  // Căn lề trong (Padding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);                // Bo góc (Rounding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);              // Độ dày viền
        ImGui::SetNextWindowBgAlpha(*pAlpha); 

        // Render Tooltip
        if (ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(text);
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(3);
    }
    inline bool ModernToggle(const char* str_id, bool* v, bool enabled = true, float scale = 1.0f) {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGuiID id = window->GetID(str_id);

        float height = 18.0f * scale;
        float width = 36.0f * scale;
        float radius = height * 0.5f;

        ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return false;

        // 1. Chỉ xử lý Click nếu enabled = true
        bool hovered, held;
        bool pressed = false;
        if (enabled) {
            pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
            if (pressed) *v = !*v;
        }

        // 2. Cập nhật Animation
        float frameTime = g.IO.DeltaTime * 12.0f;
        float* pT = window->StateStorage.GetFloatRef(id, *v ? 1.0f : 0.0f);
        
        // Nếu enabled mới cho phép chạy animation gạt qua lại
        if (enabled) {
            *pT = ImClamp(*pT + (*v ? frameTime : -frameTime), 0.0f, 1.0f);
        } else {
            // Nếu bị tắt, giữ nguyên vị trí hiện tại (hoặc ép về trạng thái thực của v)
            *pT = *v ? 1.0f : 0.0f;
        }

        // 3. Tính toán màu sắc dựa trên trạng thái enabled
        ImVec4 col_off = GTheme.FrameBg;
        ImVec4 col_on  = GTheme.CheckMark;
        
        // Nội suy màu nền
        ImVec4 current_col = ImLerp(col_off, col_on, *pT);
        
        // Nếu disabled, giảm độ đậm (Alpha) của cả background và knob
        float final_alpha = enabled ? 1.0f : 0.35f; 
        ImU32 bg_color = ImGui::ColorConvertFloat4ToU32(ImVec4(current_col.x, current_col.y, current_col.z, final_alpha));
        ImU32 knob_color = ImGui::ColorConvertFloat4ToU32(ImVec4(GTheme.Text.x, GTheme.Text.y, GTheme.Text.z, final_alpha));

        // 4. Vẽ Background & Knob
        window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_color, 10.0f);
        
        float knob_pos_x = bb.Min.x + radius + (*pT * (width - height));
        window->DrawList->AddCircleFilled(ImVec2(knob_pos_x, bb.Min.y + radius), radius - 2.0f, knob_color);

        return pressed;
    }


}
