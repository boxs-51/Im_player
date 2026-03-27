#include "utils.h"

#include <imgui_internal.h>
#include <imgui.h>

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