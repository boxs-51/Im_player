#include <gui/gui_widgets.h>
#include <gui/gui.h>
#include <cmath>
static ImVec2 FitSizeWithAspect(ImVec2 size, float max_w, float max_h, float aspect)
{
    if (size.x <= 0 || size.y <= 0)
        return size;

    float w = size.x;
    float h = size.y;

    if (w > max_w)
    {
        w = max_w;
        h = w / aspect;
    }

    if (h > max_h)
    {
        h = max_h;
        w = h * aspect;
    }

    return ImVec2(w, h);
}
void DrawPlayPauseIcon(
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
void DrawPlayIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    // Vẽ tam giác cho nút Play
    ImVec2 p1 = pMin;
    ImVec2 p2 = ImVec2(pMin.x, pMax.y);
    ImVec2 p3 = ImVec2(pMax.x, (pMin.y + pMax.y) * 0.5f);
    drawList->AddTriangleFilled(p1, p2, p3, color);
}

void DrawPauseIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
    float width = pMax.x - pMin.x;
    float barWidth = width * 0.35f;
    // Thanh bên trái
    drawList->AddRectFilled(pMin, ImVec2(pMin.x + barWidth, pMax.y), color);
    // Thanh bên phải
    drawList->AddRectFilled(ImVec2(pMax.x - barWidth, pMin.y), pMax, color);
}

void DrawPrevIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
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

void DrawNextIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
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

void DrawVolumeIcon(
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

void DrawSettingsIconAnimated(
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

void DrawFullscreenIconAnimated(
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
void DrawFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
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
void DrawUnFullscreenIcon(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color) {
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
void DrawOptionIconAnimated(
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
void DrawLoadingIconAnimated(
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


const CSImGui::IconButtonStyle& CSImGui::GetDefaultIconButtonStyle()
{
    static IconButtonStyle s;
    return s;
}
bool CSImGui::CustomIconButton(
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
void IconWrapperAdapter(
    ImDrawList* dl,
    ImVec2 min,
    ImVec2 max,
    ImU32 col,
    void* user_data)
{
    OldIconFn fn = reinterpret_cast<OldIconFn>(user_data);
    fn(dl, min, max, col);
}
bool CSImGui::CustomIconButton(
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
bool CSImGui::CustomIconButton(
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
bool CSImGui::CustomIconButton(
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
std::string safeFormatArg(const char* fmtSpec, va_list args, char type) {
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

// InfoRow an toàn, hỗ trợ std::string
void CSImGui::InfoRow(const char* label, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Kiểm tra nếu giá trị rỗng hoặc chỉ có khoảng trắng
    bool isEmpty = (len <= 0 || buf[0] == '\0');
    ImGui::Spacing();
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
    ImGui::Spacing();
    // Chỉ cho phép Copy nếu có dữ liệu
    //if (!isEmpty && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    //    ImGui::SetClipboardText(buf);
    //}
}

bool CSImGui::BeginInfoTable(const char* id, int column_count, float first_col_width, ImGuiTableFlags extra_flags) {
    // Flags: Thêm NoBordersInBody để UI trông phẳng (Flat Design)
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | 
                            ImGuiTableFlags_RowBg | 
                            ImGuiTableFlags_NoSavedSettings | 
                            ImGuiTableFlags_NoBordersInBody | 
                            extra_flags;

    // Đẩy khoảng cách giữa các ô ra một chút (Padding)
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 4.0f));
    ImGui::Spacing();
    if (ImGui::BeginTable(id, column_count, flags)) {
        ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, first_col_width);
        for (int i = 1; i < column_count; i++) {
            ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);
        }

        ImGui::PushStyleColor(ImGuiCol_TableRowBg,    GetColors(Col_TableRowBg));
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, GetColors(Col_TableRowBgAlt)); 
        
        return true;
    }
    
    ImGui::PopStyleVar(); // Pop CellPadding nếu BeginTable fail
    return false;
}

void CSImGui::EndInfoTable() {
    ImGui::EndTable();
    ImGui::Spacing();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(); // Pop CellPadding
}

bool CSImGui::BeginCard() {
    // Sử dụng màu nền Card nhẹ nhàng, tiệp với tông Dark của Window
    ImGui::PushStyleColor(ImGuiCol_ChildBg, GetColors(Col_ChildBg)); 
    ImGui::PushStyleColor(ImGuiCol_Border,  GetColors(Col_Border)); // Viền mảnh
    ImGui::PushStyleColor(ImGuiCol_Text,    GetColors(Col_Text)); // Chữ trắng sáng

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

void CSImGui::EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
}

bool CSImGui::BeginModernChild(const char* str_id, const ImVec2& size , bool border, ImGuiWindowFlags extra_flags) {

    // Tùy chỉnh Style cho hiện đại
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f); // Bo góc mềm mại
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    
    // Màu sắc (Sử dụng màu tối nhẹ hoặc trắng tinh khôi)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, GetColors(Col_ChildBg)); 
    ImGui::PushStyleColor(ImGuiCol_Border, GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));

    if(ImGui::BeginChild(str_id, size, border, extra_flags | ImGuiWindowFlags_NoScrollbar)) return true;

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);

    return false;
}

void CSImGui::EndModernChild() {
    ImGui::EndChild();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}
bool CSImGui::BeginModernTabBar(const char* id , ImGuiTabBarFlags extra_flags ) {

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(15.0f, 0.0f)); 
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f)); // Tab cao hơn nhìn sang hơn
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 5.0f);
    
    // Màu sắc
    ImGui::PushStyleColor(ImGuiCol_Text,          GetColors(Col_Text));  // Text mặc định hơi tối
    ImGui::PushStyleColor(ImGuiCol_Tab,           GetColors(Col_Tab));              // Trong suốt khi ko chọn
    ImGui::PushStyleColor(ImGuiCol_TabHovered,    GetColors(Col_TabHovered));
    ImGui::PushStyleColor(ImGuiCol_TabActive,     GetColors(Col_TabActive)); // Tiệp màu với ChildBg bên dưới
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused,  GetColors(Col_TabUnfocused));
    ImGui::PushStyleColor(ImGuiCol_Button,        GetColors(Col_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GetColors(Col_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  GetColors(Col_ButtonActive));
    
    // Đường kẻ dưới Tab Active (Màu Accent)
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, GetColors(Col_TabUnfocusedActive));
    ImGuiTabBarFlags flags = ImGuiTabBarFlags_None;
    if (!(extra_flags & ImGuiTabBarFlags_FittingPolicyMask_)) {
            flags |= ImGuiTabBarFlags_FittingPolicyScroll;
        }

    flags |= extra_flags;
    if(ImGui::BeginTabBar(id ,flags)) {
        return true;
    }

    ImGui::PopStyleColor(9);
    ImGui::PopStyleVar(3);

    return false;
}
void CSImGui::EndModernTabBar() {
    ImGui::EndTabBar();
    ImGui::PopStyleColor(9);
    ImGui::PopStyleVar(3);
}
bool CSImGui::ModernTabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags, ModernTabFlags m_flags) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(label);

    // 1. Tùy chỉnh màu sắc Tab
    ImGui::PushStyleColor(ImGuiCol_Tab,                ImVec4(0,0,0,0)); // Tab ko chọn thì trong suốt
    ImGui::PushStyleColor(ImGuiCol_TabHovered,         GetColors(Col_TabHovered));
    ImGui::PushStyleColor(ImGuiCol_TabActive,          ImVec4(0,0,0,0)); // Active cũng ko nền để hiện thanh bar
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused,       ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0,0,0,0));

    bool selected = ImGui::BeginTabItem(label, p_open, flags);

    // 2. Logic Animation cho thanh Bar bên dưới
    // Lưu ý: ImGui quản lý Tab hơi khác, ta cần dùng trạng thái đã chọn của frame trước
    if (ImGui::IsItemVisible()) {
        float* pT = ImGui::GetStateStorage()->GetFloatRef(id, 0.0f);
        if (!(m_flags & ModernTabFlags_NoAnimation)) {
            float target = ImGui::IsItemHovered() || selected ? 1.0f : 0.0f;
            *pT += (target - *pT) * g.IO.DeltaTime * 12.0f;
        }else{
            *pT = selected ? 1.0f : 0.0f;
        }

        // 3. Vẽ thanh chỉ báo (Indicator Bar)
        if (*pT > 0.01f && !(m_flags & ModernTabFlags_NoIndicator)) {
            ImVec2 p_min = ImGui::GetItemRectMin();
            ImVec2 p_max = ImGui::GetItemRectMax();
            float full_width = p_max.x - p_min.x;
            float bar_width = (m_flags & ModernTabFlags_FullWidthBar) ? full_width : full_width * 0.8f;
            float offset = (full_width - bar_width) * 0.5f;

            ImVec2 b_min = ImVec2(p_min.x + offset, p_max.y - 2.0f);
            ImVec2 b_max = ImVec2(p_min.x + offset + (bar_width * *pT), p_max.y); // Dài dần ra

            ImVec4 col = selected ? GetColors(Col_CheckMark) : GetColors(Col_TextDisabled);
            col.w *= *pT;

            window->DrawList->AddRectFilled(b_min, b_max, ImGui::GetColorU32(col), 10.0f);
        }
    }
    if(!selected){
        ImGui::PopStyleColor(5);
        return selected;
    }

    return selected;
}
void CSImGui::EndModernTabItem() {
    ImGui::EndTabItem();
    ImGui::PopStyleColor(5);
}
void CSImGui::PushModernWindowStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // 1. Bo góc và viền
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
    
    // 2. Tiêu đề (Title bar) - Làm cho nó cao hơn và phẳng hơn
    ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f)); // Căn giữa title
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));    // Tăng độ cao title bar
    
    // 3. Màu sắc hiện đại (Dark Theme tinh tế)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, GetColors(Col_WindowBg));
    ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, GetColors(Col_TitleBg));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, GetColors(Col_TitleBgActive));
    ImGui::PushStyleColor(ImGuiCol_Border, GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Separator, GetColors(Col_Separator));
}
void CSImGui::PopModernWindowStyle() {
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(5);
}
bool CSImGui::ModernButton(const char* label, const ImVec2& size_arg, bool primary) {
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
    ImVec4 col_base = primary ? GetColors(Col_Button) : ImVec4(0,0,0,0);
    ImVec4 col_hover = GetColors(Col_ButtonHovered);
    ImVec4 final_bg = ImLerp(col_base, col_hover, *pAnim);
    if (held) final_bg = GetColors(Col_ButtonActive);

    // Vẽ nền (Sử dụng DrawList để bo góc mượt)
    window->DrawList->AddRectFilled(bb.Min - ImVec2(2 * *pAnim, 1 * *pAnim), 
                                    bb.Max + ImVec2(2 * *pAnim, 1 * *pAnim), 
                                    ImGui::ColorConvertFloat4ToU32(final_bg), 6.0f);

    // Nếu là Secondary, vẽ thêm viền
    if (!primary) {
        window->DrawList->AddRect(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(GetColors(Col_Border)), 6.0f);
    }

    // Vẽ chữ căn giữa
    ImGui::RenderTextClipped(bb.Min, bb.Max, label, NULL, &label_size, ImVec2(0.5f, 0.5f));

    return pressed;
}

bool CSImGui::SecondaryButton(const char* label, const ImVec2& size) {
    return ModernButton(label, size, false);
}
// Thêm biến static để lưu ID đang được hover toàn cục
static ImGuiID G_LastHoveredID = 0;

bool CSImGui::ModernButtonEx(const char* label, const ImVec2& size_arg) {

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
    ImVec4 final_bg = ImLerp(GetColors(Col_Button), GetColors(Col_ButtonHovered), *pAnim + influence);
    // ... vẽ tiếp ...
    return true;
}

bool CSImGui::ModernCheckbox(const char* label, bool* v, CheckboxStyle style, const ImVec2& size_arg) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& imStyle = g.Style;
    const ImGuiID id = window->GetID(label);

    // 1. Tính toán kích thước Động dựa trên tham số truyền vào
    // Nếu size_arg không được truyền, mặc định lấy GetFrameHeight() của hệ thống
    float square_sz = (size_arg.x > 0.0f) ? size_arg.x : ImGui::GetFrameHeight(); 
    ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    
    ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0 ? imStyle.ItemInnerSpacing.x + label_size.x : 0), square_sz));
    
    // Đồng bộ hóa FramePadding theo tỷ lệ kích thước mới để tránh bị lệch dòng
    float padding_y = (size_arg.x > 0.0f) ? 0.0f : imStyle.FramePadding.y;
    ImGui::ItemSize(total_bb, padding_y);
    if (!ImGui::ItemAdd(total_bb, id)) return false;

    // 2. Tương tác
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    // 3. Animation tCheck
    float* pT = window->StateStorage.GetFloatRef(id, *v ? 1.0f : 0.0f);
    *pT = ImClamp(*pT + (*v ? g.IO.DeltaTime * 12.0f : -g.IO.DeltaTime * 12.0f), 0.0f, 1.0f);

    // 4. Vẽ Box nền
    float rounding = (style == CheckboxStyle::Circle) ? square_sz * 0.5f : 4.0f;
    ImU32 col_bg = ImGui::GetColorU32((held && hovered) ? GetColors(Col_FrameBgActive) : hovered ? GetColors(Col_FrameBgHovered) : GetColors(Col_FrameBg));
    
    window->DrawList->AddRectFilled(pos, pos + ImVec2(square_sz, square_sz), col_bg, rounding);
    window->DrawList->AddRect(pos, pos + ImVec2(square_sz, square_sz), ImGui::GetColorU32(ToCol32(GetColors(Col_Border)), 0.5f), rounding);

    // 5. Vẽ nội dung Check (Co giãn tỷ lệ 100% theo square_sz mới)
    if (*pT > 0.01f) {
        ImVec2 center = pos + ImVec2(square_sz * 0.5f, square_sz * 0.5f);
        ImU32 check_col = ImGui::GetColorU32(GetColors(Col_CheckMark));
        check_col = (check_col & 0x00FFFFFF) | ((uint32_t)(*pT * 255) << 24);

        if (style == CheckboxStyle::Circle) {
            window->DrawList->AddCircleFilled(center, (square_sz * 0.25f) * *pT, check_col);
        }
        else if (style == CheckboxStyle::Square) {
            float pad = square_sz * 0.25f * (1.0f - *pT + 1.0f);
            window->DrawList->AddRectFilled(pos + ImVec2(pad, pad), pos + ImVec2(square_sz - pad, square_sz - pad), check_col, 2.0f);
        }
        else if (style == CheckboxStyle::Tick) {
            // Tự động điều chỉnh độ dày nét vẽ (thickness) tỷ lệ thuận với kích thước hộp Checkbox
            float thickness = (square_sz < 18.0f) ? 1.5f : 2.0f; 
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

    // 6. Căn giữa Label theo trục Y pixel-perfect
    if (label_size.x > 0.0f) {
        float text_y_offset = (square_sz - g.FontSize) * 0.5f;
        ImVec2 text_pos = ImVec2(pos.x + square_sz + imStyle.ItemInnerSpacing.x, pos.y + text_y_offset);
        
        ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));
        ImGui::RenderText(text_pos, label);
        ImGui::PopStyleColor();
    }

    return pressed;
}

// Callback hỗ trợ riêng cho std::string
int StringResizeCallback(ImGuiInputTextCallbackData* data)
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
void RenderModernInputEffect(ImGuiID id, float* pFocusAnim) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    float target = ImGui::IsItemActive() ? 1.0f : 0.0f;
    *pFocusAnim += (target - *pFocusAnim) * ImGui::GetIO().DeltaTime * 10.0f;

    if (*pFocusAnim > 0.01f) {
        ImRect rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        rect.Expand(0.5f);
        ImVec4 accent_col = CSImGui::GetColors(Col_CheckMark);
        accent_col.w *= *pFocusAnim;
        window->DrawList->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(accent_col), 6.0f, 0, 1.5f);
    }
}
bool CSImGui::ModernInputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags) {
    ImGuiID id = ImGui::GetCurrentWindow()->GetID(label);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,          GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    GetColors(Col_FrameBgActive));
    ImGui::PushStyleColor(ImGuiCol_Border,           GetColors(Col_Border)); 
    ImGui::PushStyleColor(ImGuiCol_Text,             GetColors(Col_Text));

    bool changed = ImGui::InputTextMultiline(label, buf, buf_size, size, flags);

    float* pFocusAnim = ImGui::GetStateStorage()->GetFloatRef(id + 1, 0.0f);
    RenderModernInputEffect(id, pFocusAnim);

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
    return changed;
}
bool CSImGui::ModernInputTextMultiline(const char* label, std::string& str, const ImVec2& size, ImGuiInputTextFlags flags) {
    ImGuiID id = ImGui::GetCurrentWindow()->GetID(label);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,         GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    GetColors(Col_FrameBgActive));
    ImGui::PushStyleColor(ImGuiCol_Border,           GetColors(Col_Border)); 
    ImGui::PushStyleColor(ImGuiCol_Text,             GetColors(Col_Text));

    // 1️⃣ BẮT BUỘC: Ép flag CallbackResize
    flags |= ImGuiInputTextFlags_CallbackResize;

    // 2️⃣ ĐẢM BẢO: Chuỗi không được rỗng khi lấy con trỏ buffer, ít nhất phải có ký tự \0
    if (str.empty()) {
        str.resize(1, '\0');
    }

    // 3️⃣ CHUẨN XÁC: Định nghĩa một Lambda callback cục bộ ngay tại đây để đồng bộ std::string
    auto InputTextCallback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            std::string* local_str = (std::string*)data->UserData;
            IM_ASSERT(data->Buf == local_str->data()); // Kiểm tra an toàn vùng nhớ
            
            // Co giãn thực tế theo nhu cầu của ImGui
            local_str->resize(data->BufTextLen);
            
            // Cập nhật lại con trỏ buffer mới sau khi resize cho ImGui quản lý tiếp
            data->Buf = (char*)local_str->data();
        }
        return 0;
    };

    // 4️⃣ TRUYỀN BUFFER: Dùng str.data() thay vì (char*)str.c_str() 
    // Độ dài truyền vào là str.capacity() chứ không lấy size, để tận dụng vùng nhớ cấp phát sẵn
    bool changed = ImGui::InputTextMultiline(
        label, 
        (char*)str.data(), 
        str.capacity() + 1, 
        size, 
        flags, 
        InputTextCallback, 
        (void*)&str
    );

    // 5️⃣ ĐỒNG BỘ: Thu gọn kích thước thực tế của std::string về đúng số lượng ký tự đã nhập
    // Do ImGui chỉ ghi đè dữ liệu, ta cần cập nhật lại độ dài (length) chuẩn cho std::string
    if (changed) {
        str.resize(strlen(str.data()));
    }

    float* pFocusAnim = ImGui::GetStateStorage()->GetFloatRef(id + 1, 0.0f);
    RenderModernInputEffect(id, pFocusAnim);

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
    return changed;
}
bool CSImGui::ModernInputText(const char* label, char* buf, size_t buf_size, float width, ImGuiInputTextFlags flags) {
    ImGuiID id = ImGui::GetID(label);
    
    // Logic Animation & Glow
    float* pAnim = ImGui::GetStateStorage()->GetFloatRef(id + 500, 0.0f);
    bool is_active = ImGui::GetActiveID() == id;
    *pAnim += ((is_active ? 1.0f : 0.0f) - *pAnim) * ImGui::GetIO().DeltaTime * 12.0f;

    // Style cho ô Input
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_Border, is_active ? GetColors(Col_CheckMark) : GetColors(Col_Border));

    ImGui::SetNextItemWidth(width);
    bool changed = ImGui::InputTextEx(label, "Type here...", buf, (int)buf_size, ImVec2(width, 0), flags);

    // Vẽ hiệu ứng Glow khi Active (Focus)
    if (*pAnim > 0.01f) {
        ImRect rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        rect.Expand(1.5f * (*pAnim));
        ImVec4 glow_col = GetColors(Col_CheckMark);
        glow_col.w *= (*pAnim * 0.4f);
        ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(glow_col), 6.0f, 0, 1.5f);
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    return changed;
}
bool CSImGui::ModernInputText(const char* label, std::string& buffer, float width, ImGuiInputTextFlags flags) {
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
    
    ImGui::PushStyleColor(ImGuiCol_FrameBg, GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_Border, is_active ? GetColors(Col_CheckMark) : GetColors(Col_Border));

    ImGui::SetNextItemWidth(width);
    
    // Gọi InputText với Callback Resize
    flags |= ImGuiInputTextFlags_CallbackResize;
    bool changed = ImGui::InputTextEx(label, "Type to search...", (char*)buffer.data(), (int)buffer.capacity() + 1, ImVec2(width, 0), flags, StringResizeCallback, (void*)&buffer);

    // Vẽ hiệu ứng Glow khi Active
    if (*pAnim > 0.01f) {
        ImRect rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        rect.Expand(1.5f * (*pAnim));
        ImVec4 glow_col = GetColors(Col_CheckMark);
        glow_col.w *= (*pAnim * 0.4f);
        ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(glow_col), 6.0f, 0, 1.5f);
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    
    return changed;
}

bool CSImGui::ModernSelectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size_arg, bool enabled) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const ImGuiID id = window->GetID(label);
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    // 1. Tính toán kích thước và vị trí
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() + 8.0f);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    // 🌟 QUAN TRỌNG: Cần đăng ký Item với ImGui TRƯỚC KHI xử lý ButtonBehavior
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bb, id)) return false;

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
            ImVec4 bgColor = GetColors(Col_Header); 
            if (*pTHover > 0.0f) {
                bgColor = ImLerp(GetColors(Col_Header), GetColors(Col_HeaderHovered), *pTHover);
            }
            
            // Nếu đang được chọn, ưu tiên hiển thị màu HeaderActive hoặc giữ màu trộn
            if (selected) bgColor = GetColors(Col_HeaderActive);

            window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(bgColor), 4.0f);
        }
    }

    // 5. Vẽ Thanh Chỉ Báo (Selection Indicator) ở cạnh trái
    if (enabled && *pTSelect > 0.0f) {
        float indicatorHeight = (bb.Max.y - bb.Min.y) * 0.6f; // Cao 60% item
        float centerY = (bb.Min.y + bb.Max.y) * 0.5f;
        
        ImVec2 p1(bb.Min.x + 2.0f, centerY - (indicatorHeight * 0.5f) * *pTSelect);
        ImVec2 p2(bb.Min.x + 4.5f, centerY + (indicatorHeight * 0.5f) * *pTSelect);
        
        window->DrawList->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(GetColors(Col_Button)), 10.0f);
    }


    // 6. Vẽ Văn Bản với hiệu ứng trượt (Slide)
    if (label[0] != '#' || (label[1] != '\0' && label[1] != '#')) {
        float textOffsetX = 10.0f + (*pTHover * 4.0f); // Trượt sang phải 4px khi hover
        ImVec4 textColor = selected ? GetColors(Col_TextSelected) : GetColors(Col_Text);
        
        // Mix màu chữ mượt mà nếu cần (Tùy chọn)
        window->DrawList->AddText(ImVec2(pos.x + textOffsetX, pos.y + (size.y - g.FontSize) * 0.5f), 
                                ImGui::ColorConvertFloat4ToU32(textColor), label);
    }

    return pressed;
}

bool CSImGui::BeginListTable(const char* id, const std::vector<TableCol>& cols, ImGuiTableFlags extra_flags) {
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
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,   GetColors(Col_TableHeaderBg));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,    GetColors(Col_HeaderActive));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   GetColors(Col_HeaderHovered));
        ImGui::PushStyleColor(ImGuiCol_Text,            GetColors(Col_Text));
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor(4);

        // Màu xen kẽ hàng
        //ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, GetColors(Col_TableRowBgAlt_ListTable);
        return true;
    }
    
    ImGui::PopStyleVar(); // Pop CellPadding nếu fail
    return false;
}
void CSImGui::EndListTable() {
    ImGui::EndTable();
    //ImGui::PopStyleColor(); // Pop TableRowBgAlt
    ImGui::PopStyleVar();   // Pop CellPadding
}
bool CSImGui::BeginListRow(float height) {
    ImGui::TableNextRow(ImGuiTableRowFlags_None, height);
    
    // Tạo một ID ẩn cho hàng để bắt sự kiện click trên toàn bộ hàng
    ImGui::TableNextColumn(); 
    ImGui::PushID(ImGui::GetCursorPosY());
    
    // Trả về true nếu người dùng click vào hàng này (sẽ kiểm tra ở cuối hàng)
    return true; 
}

void CSImGui::EndListRow() {
    ImGui::PopID();
}

// Kiểm tra xem hàng vừa vẽ có được click hay không
bool CSImGui::IsRowClicked() {
    return ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped) && ImGui::IsMouseReleased(0);
}

bool CSImGui::ModernCollapsingHeader(const char* id, ImGuiTreeNodeFlags flags) {
    // Bo góc nhẹ cho header nếu muốn đồng bộ với Combo
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

    ImGui::PushStyleColor(ImGuiCol_Header,        GetColors(Col_Header));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GetColors(Col_HeaderHovered));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  GetColors(Col_HeaderActive));
    ImGui::PushStyleColor(ImGuiCol_Text,          GetColors(Col_Text)); // Dùng chung màu text cho đồng bộ

    bool res = ImGui::CollapsingHeader(id, flags);

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    return res;
}
// --- HELPER: Vẽ phần thân danh sách cho Combo (Normal & Search) ---
bool DrawComboPopupBody(
    const char* label,
    std::string& current_value, 
    const std::vector<std::string>& display_options, 
    float custom_width, 
    int max_items_visible,
    std::function<bool(std::string&)> on_validate_confirm) 
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

                if (CSImGui::ModernSelectable(truncated_opt.c_str(), is_selected)) {
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
bool CSImGui::ModernSearchCombo(const char* label,
    std::string& current_value,
    const std::vector<std::string>& options,
    float custom_width ,
    int max_items_visible,
    std::function<bool(std::string&)> on_validate_confirm ,
    std::function<std::string(const std::string&)> on_get_dynamic_opt)
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
    ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));
    ImGui::TextDisabled("%s", label);
    ImGui::PopStyleColor();

    ImGui::SetNextItemWidth(custom_width);

    // 3. Style cho ô InputText
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,         GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_Border,          GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Text,            GetColors(Col_Text));

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
        
        ImVec4 glow_col = GetColors(Col_CheckMark); 
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

    ImGui::PushStyleColor(ImGuiCol_PopupBg, GetColors(Col_PopupBg));
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

bool CSImGui::NormalCombo(const char* label, std::string& current_item, const std::vector<std::string>& options, float custom_width, int max_items_visible) {
    bool changed = false;
    ImGuiID popup_id = ImGui::GetID((std::string(label) + "_popup").c_str());
    ImGuiID anim_id = ImGui::GetID((std::string(label) + "_anim").c_str());
    ImGuiID arrow_id = ImGui::GetID((std::string(label) + "_arrow").c_str());

    bool is_open = ImGui::IsPopupOpen(popup_id,ImGuiPopupFlags_None);

    // 1. Style cho Label
    ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));
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
    ImU32 bg_col = ImGui::GetColorU32(is_open ? GetColors(Col_FrameBg) : (is_hovered ? GetColors(Col_FrameBgHovered) : GetColors(Col_FrameBg)));
    
    draw_list->AddRectFilled(p_min, p_max, bg_col, 6.0f);
    draw_list->AddRect(p_min, p_max, ImGui::GetColorU32(GetColors(Col_Border)), 6.0f, 0, 1.0f);

    // Vẽ viền sáng Glow (Giống hệt SearchCombo)
    if (*pAnim > 0.01f) {
        ImVec4 accent = GetColors(Col_CheckMark);
        accent.w *= (*pAnim * 0.4f);
        draw_list->AddRect(p_min, p_max, ImGui::GetColorU32(accent), 6.0f, 0, 1.5f);
    }

    // Vẽ Text hiển thị (Đã cắt tỉa)
    float available_text_width = custom_width - 35.0f; 
    std::string truncated_display = TextUtils::TruncateTextByPixels(current_item.c_str(), available_text_width);
    // Cộng thêm FramePadding để text nằm giữa
    draw_list->AddText(ImVec2(p_min.x + 10.0f, p_min.y + 8.0f), ImGui::GetColorU32(GetColors(Col_Text)), truncated_display.c_str());

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
    draw_list->AddTriangleFilled(p1, p2, p3, ImGui::GetColorU32(is_open ? GetColors(Col_CheckMark) : GetColors(Col_Text)));

    ImGui::PopStyleVar(2); // Dọn dẹp Frame Vars

    // --- XÂY DỰNG POPUP DANH SÁCH (Dùng chung logic ModernSearchCombo) ---
    ImGui::SetNextWindowPos(ImVec2(p_min.x, p_max.y + 2));
    ImGui::SetNextWindowSizeConstraints(ImVec2(custom_width, 0), ImVec2(custom_width, FLT_MAX));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, GetColors(Col_PopupBg));
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
bool CSImGui::BeginModernPopup(const char* name, bool* open, ImGuiWindowFlags flags) {
    
    ImGuiID id = ImGui::GetID(name);
    float* appear_anim = ImGui::GetStateStorage()->GetFloatRef(id + 10, 0.0f);
    
    // Nếu popup đang mở, tăng dần animation
    if (ImGui::IsPopupOpen(name))
        *appear_anim = ImMin(*appear_anim + ImGui::GetIO().DeltaTime * 6.0f, 1.0f);
    else
        *appear_anim = 0.0f;
    // Áp dụng Style Window hiện đại cho Popup
    CSImGui::PushModernWindowStyle();
    
    ImVec4 dim_col = ImVec4(0, 0, 0, 0.6f * (*appear_anim));
    // Thêm hiệu ứng làm mờ nền (Dim background)
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, dim_col);

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool isOpen = ImGui::BeginPopupModal(name, open, flags | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    
    if (!isOpen) {
        ImGui::PopStyleColor();
        CSImGui::PopModernWindowStyle();
    }
    return isOpen;
}

void CSImGui::EndModernPopup() {
    ImGui::EndPopup();
    ImGui::PopStyleColor();
    CSImGui::PopModernWindowStyle();
}

void CSImGui::EndModernTreeNode(){
    ImGui::TreePop();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}
bool CSImGui::ModernTreeNode(const char* id) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 6)); 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

    ImGui::PushStyleColor(ImGuiCol_Header,         GetColors(Col_Header)); 
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  GetColors(Col_HeaderHovered)); 
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,   GetColors(Col_HeaderActive)); 
    ImGui::PushStyleColor(ImGuiCol_Text,           GetColors(Col_Text)); 

    bool open = ImGui::TreeNode(id);
    
    if (!open) {
        // Nếu không mở, chúng ta phải Pop ngay tại đây
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }
    // Nếu mở, việc Pop sẽ do EndModernTreeNode đảm nhận
    return open;
}

static bool UpdateTooltipState(TooltipData* td, ToolTipFlags flags, float& out_alpha) {
    auto& item = td->item;
    auto& cfg  = td->config;
    auto  anim = td->anim;

    if (flags & ToolTipFlags_Hiden)
        item.is_visible = false;
    else if (flags & ToolTipFlags_AlwaysShow)
        item.is_visible = true;
    else if (flags & ToolTipFlags_AlwaysShowAction)
        item.is_visible = item.active;
    else
        item.is_visible = (item.hovered && item.hovered_time >= item.hover_delay);

    if (!item.is_visible) cfg.lock_dir = false;
    
    float target_alpha = item.is_visible ? 1.0f : 0.0f;
    out_alpha = target_alpha;
    
    if (anim) {
        if (flags & ToolTipFlags_Fade) {
            anim->alpha = ImLerp(anim->alpha, target_alpha, SMOOTH_LERP(anim->speedfade, cfg.dt));
            out_alpha = anim->alpha;
        } else {
            anim->alpha = target_alpha;
        }
    }
    return out_alpha > 0.01f;
}

static void ComputeTooltipLayout(TooltipData* td, ToolTipFlags flags, float alpha, TooltipCallback tooltip_cb) {
    auto& item = td->item;
    auto& cfg  = td->config;

    float seek_v = 0.0f;
    if (cfg.show_text) {
        float t_mouse = ImClamp((item.mouse.x - item.pos.x) / item.size.x, 0.0f, 1.0f);
        seek_v = item.v_min + t_mouse * item.range;
    }
    
    // =========================
    // INIT DATA
    // =========================
    if (cfg.show_text)
        item.seek_value = seek_v;

    cfg.col_bg = ImGui::GetColorU32(ImGuiCol_PopupBg);
    cfg.col_text = ImGui::GetColorU32(ImGuiCol_Text);
    cfg.col_border = ImGui::GetColorU32(ImGuiCol_Border);
    cfg.col_title = ImGui::GetColorU32(ImGuiCol_CheckMark);
    cfg.col_extra = ImGui::GetColorU32(ImGuiCol_Header);

    // default text
    if (cfg.show_text)
        cfg.text = Format(cfg.format, item.active ? item.value : item.seek_value);

    if (tooltip_cb) tooltip_cb(Phase::Init, Slot::None, td, NULL);

    cfg.col_bg = ImGui::GetColorU32(cfg.col_bg, alpha);
    cfg.col_text = ImGui::GetColorU32(cfg.col_text, alpha);
    cfg.col_border = ImGui::GetColorU32(cfg.col_border, alpha);
    cfg.col_title = ImGui::GetColorU32(cfg.col_title, alpha);
    cfg.col_extra = ImGui::GetColorU32(cfg.col_extra, alpha);

    // default size
    float max_content_w = ImMax(1.0f, cfg.max_width - cfg.padding_content * 2);
    float max_content_h = ImMax(1.0f, cfg.max_height - cfg.padding_content * 2);

    if ((cfg.last_raw_title != cfg.title) && cfg.show_title) {
        cfg.cached_title = TextUtils::WrapTextToLines(cfg.font, cfg.fontsize, cfg.title, max_content_w, cfg.title_max_lines);
        cfg.last_raw_title = cfg.title;
    }
    if ((cfg.last_raw_extra != cfg.extra) && cfg.show_extra) {
        cfg.cached_extra = TextUtils::WrapTextToLines(cfg.font, cfg.fontsize, cfg.extra, max_content_w, cfg.title_max_lines);
        cfg.last_raw_extra = cfg.extra;
    }
    if ((cfg.last_raw_text != cfg.text) && cfg.show_text) {
        cfg.cached_text = TextUtils::WrapTextToLines(cfg.font, cfg.fontsize, cfg.text, max_content_w, cfg.title_max_lines);
        cfg.last_raw_text = cfg.text;
    }
    
    if (cfg.show_image) {
        if (cfg.lock_aspect) {
            cfg.image_size = FitSizeWithAspect(
                cfg.image_size,
                cfg.max_width,
                max_content_h * 0.6f, // image không chiếm full height
                cfg.aspect_ratio
            );
        }
    } else {
        cfg.image_size = ImVec2(0, 0);
    }


    auto MeasureLines = [&](const std::vector<std::string>& lines) {
        ImVec2 res(0, 0);
        for (const auto& l : lines) {
            ImVec2 sz = cfg.font->CalcTextSizeA(cfg.fontsize, FLT_MAX, 0.0f, l.c_str());
            res.x = ImMax(res.x, sz.x);
            res.y += sz.y; // Cộng dồn chiều cao
        }
        // Thêm khoảng cách giữa các dòng (line spacing)
        if (lines.size() > 1) res.y += (lines.size() - 1) * cfg.spacing_content; 
        return res;
    };

    if (cfg.show_title)  cfg.title_size = MeasureLines(cfg.cached_title);
    if (cfg.show_extra)  cfg.extra_size = MeasureLines(cfg.cached_extra);
    if (cfg.show_text)   cfg.text_size  = MeasureLines(cfg.cached_text);

    // ===== LAYOUT =====
    float content_w = 0.0f;
    float content_h = 0.0f;

    if (cfg.layout == ImGuiTooltip::Layout_Vertical)
    {
        content_w = cfg.text_size.x;
        if (cfg.show_title) content_w = ImMax(content_w, cfg.title_size.x);
        if (cfg.show_extra) content_w = ImMax(content_w, cfg.extra_size.x);
        if (cfg.show_image) content_w = ImMax(content_w, cfg.image_size.x);

        if (cfg.show_image) content_h += cfg.image_size.y + cfg.spacing_content;
        if (cfg.show_title) content_h += cfg.title_size.y + cfg.spacing_content;
        
        content_h += cfg.text_size.y; 
        
        if (cfg.show_extra) content_h += cfg.spacing_content + cfg.extra_size.y;
    }
    else // Horizontal
    {
        float text_block_h = cfg.text_size.y;
        float text_block_w = cfg.text_size.x;
        
        if (cfg.show_title) {
            text_block_h += cfg.title_size.y + cfg.spacing_content;
            text_block_w = ImMax(text_block_w, cfg.title_size.x);
        }
        if (cfg.show_extra) {
            text_block_h += cfg.extra_size.y + cfg.spacing_content;
            text_block_w = ImMax(text_block_w, cfg.extra_size.x);
        }

        content_w = text_block_w;
        content_h = text_block_h;

        if (cfg.show_image) {
            content_w += cfg.image_size.x + cfg.spacing_content;
            content_h = ImMax(content_h, cfg.image_size.y);
        }
    }

    content_w = ImMin(content_w, max_content_w);
    content_h = ImMin(content_h, max_content_h);

    content_w = ImMax(content_w, 1.0f);
    content_h = ImMax(content_h, 1.0f);

    td->out_size = ImVec2(
        content_w + cfg.padding_content * 2,
        content_h + cfg.padding_content * 2
    );
}
static void ComputeTooltipPosition(TooltipData* td, ToolTipFlags flags) {
    auto& item = td->item;
    auto& cfg  = td->config;

    ImVec2 view_min = ImGui::GetMainViewport()->Pos;
    ImVec2 view_max = view_min + ImGui::GetMainViewport()->Size;
    ImVec2 limit_min = view_min;
    ImVec2 limit_max = view_max;

    auto GetDirVector = [&](ImGuiTooltip::TooltipDirection dir)
    {
        switch (dir)
        {
        case ImGuiTooltip::Dir_Up:        return ImVec2(0, -1);
        case ImGuiTooltip::Dir_Down:      return ImVec2(0, 1);
        case ImGuiTooltip::Dir_Left:      return ImVec2(-1, 0);
        case ImGuiTooltip::Dir_Right:     return ImVec2(1, 0);
        case ImGuiTooltip::Dir_UpLeft:    return ImVec2(-1, -1);
        case ImGuiTooltip::Dir_UpRight:   return ImVec2(1, -1);
        case ImGuiTooltip::Dir_DownLeft:  return ImVec2(-1, 1);
        case ImGuiTooltip::Dir_DownRight: return ImVec2(1, 1);
        }
        return ImVec2(0, -1);
    };

    // --- Lambda ComputeBestPosition ---
    auto ComputeBestPosition = [&](ImVec2 target, ImVec2& out_pos, ImGuiTooltip::TooltipDirection& out_dir)
    {
        ImVec2 item_min = item.pos;
        ImVec2 item_max = item.pos + item.size;

        struct Candidate {
            ImVec2 pos;
            ImGuiTooltip::TooltipDirection dir;
            float score;
        };

        Candidate best = {};
        best.score = -FLT_MAX;

        auto Test = [&](ImVec2 pos, ImGuiTooltip::TooltipDirection dir)
        {
            ImVec2 pmax = pos + td->out_size;

            float overflow =
                ImMax(0.0f, view_min.x - pos.x) +
                ImMax(0.0f, view_min.y - pos.y) +
                ImMax(0.0f, pmax.x - view_max.x) +
                ImMax(0.0f, pmax.y - view_max.y);

            bool overlap =
                !(pmax.x < item_min.x || pos.x > item_max.x ||
                pmax.y < item_min.y || pos.y > item_max.y);

            float score = -overflow * 10.0f;
            if (!overlap) score += 1000.0f;

            if (score > best.score) {
                best = {pos, dir, score};
            }
        };

        // TOP
        Test(ImVec2(target.x - td->out_size.x * 0.5f, item_min.y - td->out_size.y - cfg.spacing_mouse), ImGuiTooltip::Dir_Up);
        // BOTTOM
        Test(ImVec2(target.x - td->out_size.x * 0.5f, item_max.y + cfg.spacing_mouse), ImGuiTooltip::Dir_Down);
        // RIGHT
        Test(ImVec2(item_max.x + cfg.spacing_mouse, target.y - td->out_size.y * 0.5f), ImGuiTooltip::Dir_Right);
        // LEFT
        Test(ImVec2(item_min.x - td->out_size.x - cfg.spacing_mouse, target.y - td->out_size.y * 0.5f), ImGuiTooltip::Dir_Left);
        
        Test(ImVec2(item_min.x - td->out_size.x - cfg.spacing_mouse, item_min.y - td->out_size.y), ImGuiTooltip::Dir_UpLeft);
        Test(ImVec2(item_max.x + cfg.spacing_mouse, item_min.y - td->out_size.y), ImGuiTooltip::Dir_UpRight);
        Test(ImVec2(item_min.x - td->out_size.x - cfg.spacing_mouse, item_max.y), ImGuiTooltip::Dir_DownLeft);
        Test(ImVec2(item_max.x + cfg.spacing_mouse, item_max.y), ImGuiTooltip::Dir_DownRight);

        out_pos = best.pos;
        out_dir = best.dir;
    };

    ImVec2 base_pos;
    ImGuiTooltip::TooltipDirection base_dir = ImGuiTooltip::Dir_Up;

    // ===== PRIORITY =====
    if (flags & ToolTipFlags_Fixed)
    {
        base_dir = ImGuiTooltip::Dir_Up;
        base_pos = ImVec2(
            item.center.x - td->out_size.x * 0.5f,
            item.center.y - td->out_size.y - cfg.spacing_mouse
        );
        cfg.lock_dir = false;
    }
    else if (flags & ToolTipFlags_AutoPosition)
    {
        ImVec2 target = item.mouse;

        if (flags & ToolTipFlags_FollowMouse_Fixed_X) target.x = item.center.x;
        if (flags & ToolTipFlags_FollowMouse_Fixed_Y) target.y = item.center.y;

        if (!cfg.lock_dir)
        {
            ImVec2 tmp_pos;
            ImGuiTooltip::TooltipDirection tmp_dir;

            ComputeBestPosition(target, tmp_pos, tmp_dir);

            cfg.dir = tmp_dir;
            cfg.lock_dir = true;
        }

        ImVec2 dir_vec = GetDirVector(cfg.dir);
        float len = sqrtf(dir_vec.x * dir_vec.x + dir_vec.y * dir_vec.y);
        float safe_pad = 6.0f;
        ImVec2 safe = item.cursor_size + ImVec2(safe_pad, safe_pad);
        if (len > 0.0f) dir_vec /= len;
        
        ImVec2 extra(0,0);
        if (dir_vec.x > 0) extra.x += safe.x;
        if (dir_vec.x < 0) extra.x -= safe.x;
        if (dir_vec.y > 0) extra.y += safe.y;
        if (dir_vec.y < 0) extra.y -= safe.y;

        ImVec2 size_offset(
            (dir_vec.x == 0 ? -td->out_size.x * 0.5f : (dir_vec.x < 0 ? -td->out_size.x : 0)),
            (dir_vec.y == 0 ? -td->out_size.y * 0.5f : (dir_vec.y < 0 ? -td->out_size.y : 0))
        );
        base_pos = target 
                + dir_vec * cfg.spacing_mouse 
                + extra
                + size_offset;
        base_dir = cfg.dir;
    }   
    else
    {
        ImVec2 target = item.mouse;

        if (flags & ToolTipFlags_FollowMouse_Fixed_X) target.x = item.center.x;
        if (flags & ToolTipFlags_FollowMouse_Fixed_Y) target.y = item.center.y;

        base_pos = target - ImVec2(td->out_size.x * 0.5f, td->out_size.y + cfg.spacing_mouse);
        base_dir = ImGuiTooltip::Dir_Up;

        cfg.lock_dir = false;
    }

    // APPLY
    td->out_pos = base_pos;
    cfg.dir = base_dir;

    if (flags & ToolTipFlags_ClampItem) {
        limit_min = item.pos;
        limit_max = item.pos + item.size;

        if (item.size.x < td->out_size.x) {
            float cx = item.pos.x + item.size.x * 0.5f;
            limit_min.x = cx - td->out_size.x * 0.5f;
            limit_max.x = cx + td->out_size.x * 0.5f;
        }
        if (item.size.y < td->out_size.y) {
            float cy = item.pos.y + item.size.y * 0.5f;
            limit_min.y = cy - td->out_size.y * 0.5f;
            limit_max.y = cy + td->out_size.y * 0.5f;
        }
    }
    bool is_x_fixed = (flags & ToolTipFlags_Fixed) || (flags & ToolTipFlags_FollowMouse_Fixed_X);
    bool is_y_fixed = (flags & ToolTipFlags_Fixed) || (flags & ToolTipFlags_FollowMouse_Fixed_Y);

    if (flags & ToolTipFlags_ClampItem) {
        if (!is_x_fixed) 
            td->out_pos.x = ImClamp(td->out_pos.x, limit_min.x + cfg.padding, limit_max.x - td->out_size.x - cfg.padding);
        
        if (!is_y_fixed)
            td->out_pos.y = ImClamp(td->out_pos.y, limit_min.y + cfg.padding, limit_max.y - td->out_size.y - cfg.padding);
    }
    
    // clamp
    if (flags & ToolTipFlags_ClampWindow){
        td->out_pos.x = ImClamp(td->out_pos.x, view_min.x + cfg.padding, view_max.x - td->out_size.x - cfg.padding);
        td->out_pos.y = ImClamp(td->out_pos.y, view_min.y + cfg.padding, view_max.y - td->out_size.y - cfg.padding);
    }
}

static bool BeginTooltipWindow(TooltipData* td, const ImVec2& draw_pos, const ImVec2& draw_size) {
    auto& cfg = td->config;

    ImGui::SetNextWindowPos(draw_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(draw_size, ImGuiCond_Always);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar 
                                  | ImGuiWindowFlags_NoResize 
                                  | ImGuiWindowFlags_NoMove 
                                  | ImGuiWindowFlags_NoSavedSettings 
                                  | ImGuiWindowFlags_AlwaysAutoResize 
                                  | ImGuiWindowFlags_Tooltip 
                                  | ImGuiWindowFlags_NoBackground
                                  | ImGuiWindowFlags_NoScrollbar
                                  | ImGuiWindowFlags_NoScrollWithMouse;

    char window_name[64];
    ImFormatString(window_name, sizeof(window_name), "##custom_tooltip_%08X", cfg.id);

    if (!ImGui::Begin(window_name, NULL, window_flags)) {
        ImGui::End();
        return false; // Render thất bại hoặc bị kẹp
    }

    cfg.draw_list = ImGui::GetWindowDrawList();
    
    // Ghi đè trực tiếp để tránh độ trễ 1 frame
    ImGuiWindow* current_window = ImGui::GetCurrentWindow();
    current_window->Pos = draw_pos;
    current_window->Size = draw_size;
    current_window->SizeFull = draw_size;
    current_window->DC.CursorStartPos = draw_pos + ImVec2(cfg.padding_content, cfg.padding_content);
    
    // Mở rộng ClipRect
    current_window->DrawList->PushClipRect(
        ImVec2(draw_pos.x - 50.0f, draw_pos.y - 50.0f), 
        ImVec2(draw_pos.x + draw_size.x + 50.0f, draw_pos.y + draw_size.y + 50.0f)
    );

    return true;
}
static void RenderTooltipBackground(TooltipData* td, ToolTipFlags flags, float scale, const ImVec2& center, const ImVec2& pmin, const ImVec2& pmax) {
    auto& item = td->item;
    auto& cfg  = td->config;
    auto  anim = td->anim;

    if (cfg.skip_draw) return;

    // =========================
    // ARROW + ANCHOR (360°)
    // =========================
    float aw = 5.0f * scale;
    float ah = 5.0f * scale;
    float r  = cfg.rounding;

    ImVec2 ref = (flags & (ToolTipFlags_FollowMouse | ToolTipFlags_FollowMouse_Fixed_Y)) 
        ? item.mouse : item.center;

    // Smooth dir
    ImVec2 target_dir = ref - center;
    float dist = ImLength(target_dir);
    ImVec2 dir = target_dir;
    if (dist > 0.0f) dir /= dist;
    
    if(anim && (flags & ToolTipFlags_Ease)) {
        anim->last_arrow_dir = ImLerp(
            anim->last_arrow_dir,
            dir,
            SMOOTH_LERP(anim->speedease, cfg.dt)
        );
        dir = anim->last_arrow_dir;
        float len = ImLength(dir);
        if (len > 0.0f) dir /= len;
    }
    
    float k = ImClamp(dist / 200.0f, 0.0f, 1.0f);
    k = k * k * (3.0f - 2.0f * k); // smoothstep

    float aw_dyn = ImLerp(cfg.arrow_width * cfg.arrow_scale_min * scale, cfg.arrow_width * cfg.arrow_scale_max * scale, k);
    float ah_dyn = ImLerp(cfg.arrow_height * cfg.arrow_scale_min * scale, cfg.arrow_height * cfg.arrow_scale_max * scale, k);

    float dot = fabsf(dir.x * dir.y);
    if (dot > 0.45f) {
        aw_dyn *= 0.7f;
        ah_dyn *= 0.7f;
    }

    // =========================
    // PROJECT TO RECT
    // =========================
    float hx = (pmax.x - pmin.x) * 0.5f;
    float hy = (pmax.y - pmin.y) * 0.5f;

    float tx = (fabsf(dir.x) > 1e-5f) ? hx / fabsf(dir.x) : FLT_MAX;
    float ty = (fabsf(dir.y) > 1e-5f) ? hy / fabsf(dir.y) : FLT_MAX;

    float t = ImMin(tx, ty);
    ImVec2 anchor = center + dir * t;

    // =========================
    // DETECT REGION
    // =========================
    float eps = ImMax(0.5f, scale * 0.5f);

    bool near_left   = fabs(anchor.x - pmin.x) < eps;
    bool near_right  = fabs(anchor.x - pmax.x) < eps;
    bool near_top    = fabs(anchor.y - pmin.y) < eps;
    bool near_bottom = fabs(anchor.y - pmax.y) < eps;

    bool is_corner =
        (near_top && near_left) ||
        (near_top && near_right) ||
        (near_bottom && near_left) ||
        (near_bottom && near_right);

    // =========================
    // TANGENT / NORMAL
    // =========================
    ImVec2 tangent, normal;
    ImVec2 corner_center;
    
    if (is_corner && r > 0.0f) {
        if (near_top && near_left)        corner_center = {pmin.x + r, pmin.y + r};
        else if (near_top && near_right)  corner_center = {pmax.x - r, pmin.y + r};
        else if (near_bottom && near_left)corner_center = {pmin.x + r, pmax.y - r};
        else                              corner_center = {pmax.x - r, pmax.y - r};

        ImVec2 v = anchor - corner_center;
        float l = ImLength(v);
        if (l > 0.0f) v /= l;
        anchor = corner_center + v * r;
        normal = v;
        tangent = ImVec2(-normal.y, normal.x);
    } else {
        enum Edge {Top, Bottom, Left, Right};
        Edge edge;

        if (near_top) edge = Top;
        else if (near_bottom) edge = Bottom;
        else if (near_left) edge = Left;
        else edge = Right;

        float min_x = pmin.x + r + aw_dyn;
        float max_x = pmax.x - r - aw_dyn;
        float min_y = pmin.y + r + aw_dyn;
        float max_y = pmax.y - r - aw_dyn;

        if (edge == Top || edge == Bottom)
            anchor.x = ImClamp(anchor.x, min_x, max_x);
        else
            anchor.y = ImClamp(anchor.y, min_y, max_y);

        if (edge == Top)        { tangent = ImVec2(1,0); normal = ImVec2(0,-1); }
        else if (edge == Bottom){ tangent = ImVec2(1,0); normal  = ImVec2(0,1); }
        else if (edge == Left)  { tangent = ImVec2(0,1); normal  = ImVec2(-1,0);}
        else                    { tangent = ImVec2(0,1); normal = ImVec2(1,0); }
    }

    // =========================
    // ARROW POINTS
    // =========================
    ImVec2 arrow_p1 = anchor - tangent * aw_dyn;
    ImVec2 arrow_p2 = anchor + tangent * aw_dyn;
    ImVec2 arrow_p3 = anchor + normal  * ah_dyn;

    // =========================
    // DRAW PATH
    // =========================
    bool draw_arrow = !(flags & ToolTipFlags_NoArrow) && !cfg.skip_draw_arrow;
    ImDrawList* dl = cfg.draw_list;

    auto BuildPath = [&]()
    {
        dl->PathClear();

        const int ARC_SEG = 8;
        auto Arc = [&](ImVec2 c, float a0, float a1) {
            dl->PathArcTo(c, r, a0, a1, ARC_SEG);
        };

        // 1. ---- TOP EDGE
        if (draw_arrow && near_top && !is_corner) {
            float left  = pmin.x + r;
            float right = pmax.x - r;
            float x1 = ImClamp(arrow_p1.x, left, right);
            float x2 = ImClamp(arrow_p2.x, left, right);

            dl->PathLineTo(ImVec2(x1, pmin.y));
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(ImVec2(x2, pmin.y));
            dl->PathLineTo(ImVec2(pmax.x - r, pmin.y));
        } else {
            dl->PathLineTo(ImVec2(pmin.x + r, pmin.y)); 
            dl->PathLineTo(ImVec2(pmax.x - r, pmin.y)); 
        }

        // 2. ---- TOP RIGHT CORNER
        Arc({pmax.x - r, pmin.y + r}, IM_PI*1.5f, IM_PI*2.0f);

        // 3. ---- RIGHT EDGE
        if (draw_arrow && near_right && !is_corner) {
            dl->PathLineTo(arrow_p1);
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(arrow_p2);
            dl->PathLineTo(ImVec2(pmax.x, pmax.y - r));
        } else {
            dl->PathLineTo(ImVec2(pmax.x, pmax.y - r));
        }

        // 4. ---- BOTTOM RIGHT CORNER
        Arc({pmax.x - r, pmax.y - r}, 0.0f, IM_PI*0.5f);

        // 5. ---- BOTTOM EDGE
        if (draw_arrow && near_bottom && !is_corner) {
            dl->PathLineTo(ImVec2(arrow_p2.x, pmax.y));
            dl->PathLineTo(arrow_p2);
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(arrow_p1);
            dl->PathLineTo(ImVec2(pmin.x + r, pmax.y));
        } else {
            dl->PathLineTo(ImVec2(pmin.x + r, pmax.y));
        }

        // 6. ---- BOTTOM LEFT CORNER
        Arc({pmin.x + r, pmax.y - r}, IM_PI*0.5f, IM_PI);

        // 7. ---- LEFT EDGE
        if (draw_arrow && near_left && !is_corner) {
            dl->PathLineTo(ImVec2(pmin.x, arrow_p2.y));
            dl->PathLineTo(arrow_p2);
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(arrow_p1);
            dl->PathLineTo(ImVec2(pmin.x, pmin.y + r));
        } else {
            dl->PathLineTo(ImVec2(pmin.x, pmin.y + r));
        }

        // 8. ---- TOP LEFT CORNER
        Arc({pmin.x + r, pmin.y + r}, IM_PI, IM_PI*1.5f);

        // =========================
        // CORNER ARROW
        // =========================
        if (draw_arrow && is_corner) {
            float angle = atan2f(anchor.y - corner_center.y, anchor.x - corner_center.x);
            float delta = cfg.arrow_corner_bias * (ah_dyn / r);
            float a0, a1;

            if (near_top && near_left)         { a0 = IM_PI; a1 = IM_PI*1.5f; }
            else if (near_top && near_right)   { a0 = IM_PI*1.5f; a1 = IM_PI*2.0f; }
            else if (near_bottom && near_right){ a0 = 0.0f; a1 = IM_PI*0.5f; }
            else                               { a0 = IM_PI*0.5f; a1 = IM_PI; }

            dl->PathArcTo(corner_center, r, a0, angle - delta);
            dl->PathLineTo(arrow_p1);
            dl->PathLineTo(arrow_p3);
            dl->PathLineTo(arrow_p2);
            dl->PathArcTo(corner_center, r, angle + delta, a1);
        }
    };
    BuildPath();
    if (!cfg.skip_draw_bg)
        dl->PathFillConvex(cfg.col_bg);

    if (!cfg.skip_draw_border && !(flags & ToolTipFlags_NoBorder))
    {
        BuildPath();
        dl->PathStroke(cfg.col_border, ImDrawFlags_Closed, cfg.border_thickness);
    }    
}
static void RenderTooltipContent(TooltipData* td, const ImVec2& pmin, const ImVec2& pmax, const ImVec2& scaled) {
    auto& cfg = td->config;

    if (cfg.skip_draw_text || cfg.skip_draw) return;

    float pos_y = pmin.y + cfg.padding_content;
    
    auto DrawWrappedLines = [&](const std::vector<std::string>& lines, ImU32 color) {
        for (const auto& line : lines) {
            ImVec2 sz = cfg.font->CalcTextSizeA(cfg.fontsize, FLT_MAX, 0.0f, line.c_str());
            float draw = pmin.x + cfg.padding_content;
            if (cfg.align == ImGuiTooltip::Align_Center)
                draw += (scaled.x - sz.x) * 0.5f - cfg.padding_content;
            else if (cfg.align == ImGuiTooltip::Align_Right)
                draw += (scaled.x - sz.x);

            cfg.draw_list->AddText(cfg.font, cfg.fontsize, ImVec2(draw, pos_y), color, line.c_str());
            pos_y += sz.y + cfg.spacing_content;
        }
    };

    if (cfg.layout == ImGuiTooltip::Layout_Vertical) {
        if (cfg.show_image && cfg.image) {
            ImVec2 img_pos(
                pmin.x + (scaled.x - cfg.image_size.x) * 0.5f,
                pos_y
            );
            cfg.draw_list->AddImage(cfg.image, img_pos, img_pos + cfg.image_size);
            pos_y += cfg.image_size.y + cfg.spacing_content;
        }

        if (cfg.show_title) DrawWrappedLines(cfg.cached_title, cfg.col_title);
        if (cfg.show_text)  DrawWrappedLines(cfg.cached_text, cfg.col_text);
        if (cfg.show_extra) DrawWrappedLines(cfg.cached_extra, cfg.col_extra);
    } 
    else if (cfg.layout == ImGuiTooltip::Layout_Horizontal) {
        float current_x = pmin.x + cfg.padding_content;
        float current_y = pmin.y + cfg.padding_content;

        if (cfg.show_image && cfg.image) {
            float img_y = pmin.y + (scaled.y - cfg.image_size.y) * 0.5f;
            ImVec2 img_pos(current_x, img_y);
            cfg.draw_list->AddImage(cfg.image, img_pos, img_pos + cfg.image_size);
            current_x += cfg.image_size.x + cfg.spacing_content;
        }

        float text_column_w = pmax.x - cfg.padding_content - current_x;
        float total_text_h = cfg.text_size.y;
        if (cfg.show_title) total_text_h += cfg.title_size.y + cfg.spacing_content;
        if (cfg.show_extra) total_text_h += cfg.extra_size.y + cfg.spacing_content;
        
        float start_text_y = pmin.y + (scaled.y - total_text_h) * 0.5f;

        auto DrawLinesHorizontal = [&](const std::vector<std::string>& lines, ImU32 color, float& y_offset) {
            for (const auto& line : lines) {
                ImVec2 sz = cfg.font->CalcTextSizeA(cfg.fontsize, FLT_MAX, 0.0f, line.c_str());
                float draw_x = current_x;

                if (cfg.align == ImGuiTooltip::Align_Center)
                    draw_x += (text_column_w - sz.x) * 0.5f;
                else if (cfg.align == ImGuiTooltip::Align_Right)
                    draw_x += (text_column_w - sz.x);

                cfg.draw_list->AddText(cfg.font, cfg.fontsize, ImVec2(draw_x, y_offset), color, line.c_str());
                y_offset += sz.y + 2.0f; 
            }
        };

        float run_y = start_text_y;

        if (cfg.show_title) {
            DrawLinesHorizontal(cfg.cached_title, cfg.col_title, run_y);
            run_y += cfg.spacing_content;
        }
        if (cfg.show_text)
            DrawLinesHorizontal(cfg.cached_text, cfg.col_text, run_y);

        if (cfg.show_extra) {
            run_y += cfg.spacing_content;
            DrawLinesHorizontal(cfg.cached_extra, cfg.col_extra, run_y);
        }
    }
}
static void ToolTipEx(TooltipData* td, ToolTipFlags flags = ToolTipFlags_None, TooltipCallback tooltip_cb = nullptr) {
    if (tooltip_cb) tooltip_cb(Phase::AfterInit, Slot::None, td, NULL);
    
    auto& cfg  = td->config;
    auto  anim = td->anim;

    float alpha = 0.0f;
    UpdateTooltipState(td, flags, alpha);
    
    if (alpha > 0.01f) {
        ComputeTooltipLayout(td, flags, alpha, tooltip_cb);
        ComputeTooltipPosition(td, flags);

        ImVec2 draw_pos = td->out_pos, draw_size = td->out_size;
        
        if (flags & ToolTipFlags_Ease && anim) {
            anim->pos = ImLerp(anim->pos, td->out_pos, SMOOTH_LERP(anim->speedease, cfg.dt));
            anim->size = ImLerp(anim->size, td->out_size, SMOOTH_LERP(anim->speedease, cfg.dt));
            draw_pos = anim->pos; 
            draw_size = anim->size;
        }
        
        if(BeginTooltipWindow(td, draw_pos, draw_size)) {
            float scale = 1.0f;
            if (flags & ToolTipFlags_Scale) {
                scale = 0.85f + 0.15f * alpha;
            }

            // Tính toán khung hình học cơ bản truyền xuống cho Render
            ImVec2 center = draw_pos + draw_size * 0.5f;
            ImVec2 half = (draw_size * scale) * 0.5f;
            ImVec2 pmin = center - half;
            ImVec2 pmax = center + half;
            ImVec2 scaled = ImVec2((pmax.x - pmin.x), (pmax.y - pmin.y));

            // Layer 0: Background & Arrow
            if (tooltip_cb) tooltip_cb(Phase::Draw, Slot::Draw_layer0, td, cfg.draw_list);
            RenderTooltipBackground(td, flags, scale, center, pmin, pmax);

            // Layer 1: Content (Text/Image)
            if (tooltip_cb) tooltip_cb(Phase::Draw, Slot::Draw_layer1, td, cfg.draw_list);
            RenderTooltipContent(td, pmin, pmax, scaled);

            cfg.draw_list->PopClipRect();
            ImGui::End();
        }
    }
}

static void SliderRenderBar(float* v, SliderState* state, SliderRenderData* rd, SliderFlags flags = SliderFlags_None ,SliderRenderCallback render_cb = nullptr){
    
    bool is_expanding = rd->bar_hovered || rd->active || rd->grab_hovered;
    float visual_height = state->height + rd->height_on_hover; 
    if(flags & SliderFlags_SliderEase && rd->anim){
        rd->anim->bar_height_scale = ImLerp(rd->anim->bar_height_scale, is_expanding ? 1.0f : 0.0f, SMOOTH_LERP(rd->anim->speedease, state->dt));
        visual_height = state->height + (rd->height_on_hover * rd->anim->bar_height_scale);
    }
    if(visual_height > state->height_max) visual_height = state->height_max;

    ImVec2 p1(state->pos.x, state->grab_center.y - visual_height * 0.5f);
    ImVec2 p2(state->pos.x + state->size.x, state->grab_center.y + visual_height * 0.5f);
    ImVec2 fill_p2(p1.x + state->anim_t * state->size.x, p2.y);

    // geometry
    rd->track_p1 = p1;
    rd->track_p2 = p2;
    rd->fill_p2  = fill_p2;
    rd->grab_center = state->grab_center;

    // size
    rd->height = state->height;
    rd->grab_radius = state->grab_radius;
    rd->track_height = visual_height;

    // value
    rd->value = *v;
    rd->t = state->t;
    rd->visual_t = state->anim_t;

    // state
    rd->active = state->active;
    rd->hovered = state->hovered;
    rd->bar_hovered = state->bar_hovered;
    rd->grab_hovered = state->grab_hovered;

    // default colors
    rd->col_track = state->bar_hovered
        ? ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered)
        : ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

    rd->col_fill = ImGui::GetStyleColorVec4(
        rd->active ? ImGuiCol_PlotHistogramActive : ImGuiCol_PlotHistogram);

    rd->col_border = ImGui::GetStyleColorVec4(ImGuiCol_Border);

    rd->col_grab = rd->active
        ? ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabActive)
        : rd->grab_hovered
        ? ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabHovered)
        : ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    rd->col_grab_border = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        
    rd->col_buffer = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
    rd->col_marker = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    rd->col_chapter_range = ImGui::GetStyleColorVec4(ImGuiCol_Text);

    if(flags & SliderFlags_SliderEase && rd->anim){
        rd->anim->hover = ImLerp(rd->anim->hover, rd->hovered ? 1.0f : 0.0f, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->active = ImLerp(rd->anim->active, rd->active ? 1.0f : 0.0f, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->grab_hover = ImLerp(rd->anim->grab_hover, rd->grab_hovered ? 1.0f : 0.0f, SMOOTH_LERP(rd->anim->speedease, state->dt));
         rd->anim->grab_scale = 
            rd->active ?  1.3f :
            rd->grab_hovered ? 1.15f :
        1.0f;
    }
    if (render_cb)render_cb(Phase::Init, Slot::None, state, rd, nullptr);

    ImVec4 col_track = rd->col_track;
    ImVec4 col_fill = rd->col_fill;
    ImVec4 col_grab = rd->col_grab;
    ImVec4 col_buffer = rd->col_buffer;
    ImVec4 col_border = rd->col_border;
    ImVec4 col_grab_border = rd->col_grab_border;
    ImVec4 col_marker = rd->col_marker;
    ImVec4 col_chapter_range = rd->col_chapter_range;
    ImVec4 col_grab_shadow = rd->col_grab_shadow;

    float grab_scale =  rd->active ?  1.3f :
                        rd->grab_hovered ? 1.15f :
                        1.0f;;
    if(flags & SliderFlags_SliderEase && rd->anim){
        rd->anim->col_track = ImLerp(rd->anim->col_track, rd->col_track, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_fill = ImLerp(rd->anim->col_fill, rd->col_fill, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_grab = ImLerp(rd->anim->col_grab, rd->col_grab, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_buffer = ImLerp(rd->anim->col_buffer, rd->col_buffer, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_border = ImLerp(rd->anim->col_border, rd->col_border, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_grab_border = ImLerp(rd->anim->col_grab_border, rd->col_grab_border, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_marker = ImLerp(rd->anim->col_marker, rd->col_marker, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_chapter_range = ImLerp(rd->anim->col_chapter_range, rd->col_chapter_range, SMOOTH_LERP(rd->anim->speedease, state->dt));
        rd->anim->col_grab_shadow = ImLerp(rd->anim->col_grab_shadow, rd->col_grab_shadow, SMOOTH_LERP(rd->anim->speedease, state->dt));
        
        rd->anim->grab_scale = 1.0f + 0.3f * rd->anim->active + 0.15f * rd->anim->grab_hover;

        col_track = rd->anim->col_track;
        col_fill = rd->anim->col_fill;
        col_grab = rd->anim->col_grab;
        col_buffer = rd->anim->col_buffer;
        col_border = rd->anim->col_border;
        col_grab_border = rd->anim->col_grab_border;
        col_marker = rd->anim->col_marker;
        col_chapter_range = rd->anim->col_chapter_range;
        col_grab_shadow = rd->anim->col_grab_shadow;

        grab_scale = rd->anim->grab_scale;

    }


    float radius = rd->grab_radius * grab_scale;
    radius = ImClamp(radius, 1.0f, state->height_max * 0.5f);
    if (render_cb)render_cb(Phase::Draw, Slot::Draw_layer0, state, rd, state->draw_list);
    // =========================
    // DEFAULT RENDER (SMART)
    // =========================

    // =========================
    // TRACK
    // =========================
    if (!rd->skip_draw_track && !rd->skip_draw)
    {
        state->draw_list->AddRectFilled(
            rd->track_p1, rd->track_p2,
            ToCol32(col_track),
            rd->track_height * 0.5f
        );
    }
    // =========================
    // BUFFER
    // =========================
    if(!rd->skip_draw_buffer && rd->buffer_t >= 0.0f && !rd->skip_draw)
    {
        float bt = ImClamp(rd->buffer_t, 0.0f, 1.0f);
        float x = ImLerp(rd->track_p1.x, rd->track_p2.x, bt);

        state->draw_list->AddRectFilled(
            rd->track_p1,
            ImVec2(x, rd->track_p2.y),
            ToCol32(rd->col_buffer),
            rd->track_height * 0.5f
        );
    }
    if (rd->chapter_range_end > rd->chapter_range_start && rd->chapter_range_start >= 0.0f && !rd->skip_draw_chapter_range && !rd->skip_draw)
    {
        float x1 = ImLerp(rd->track_p1.x, rd->track_p2.x, rd->chapter_range_start);
        float x2 = ImLerp(rd->track_p1.x, rd->track_p2.x, rd->chapter_range_end);

        state->draw_list->AddRectFilled(
            ImVec2(x1, rd->track_p1.y),
            ImVec2(x2, rd->track_p2.y),
            ToCol32(rd->col_chapter_range),
            rd->track_height * 0.5f
        );
    }
    // =========================
    // FILL
    // =========================
    if (!rd->skip_draw_fill && !rd->skip_draw)
    {
        state->draw_list->AddRectFilled(
            rd->track_p1, rd->fill_p2,
            ToCol32(col_fill),
            rd->track_height * 0.5f
        );
    }
    if(render_cb)render_cb(Phase::Draw, Slot::Draw_layer1, state, rd, state->draw_list);
    // =========================
    // MARKERS (NEW)
    // =========================
    if (!rd->skip_draw_markers && !rd->markers.empty() && !rd->skip_draw)
    {
        for (float m : rd->markers)
        {
            float mt = ImClamp(m, 0.0f, 1.0f);
            float x = ImLerp(rd->track_p1.x, rd->track_p2.x, mt);

            state->draw_list->AddLine(
                ImVec2(x, rd->track_p1.y),
                ImVec2(x, rd->track_p2.y),
                ToCol32(rd->col_marker),
                rd->marker_thickness
            );
        }
    }
    // =========================
    // BORDER
    // =========================

    if (!rd->skip_draw_track && !rd->skip_draw_border && !rd->skip_draw) 
    {
        state->draw_list->AddRect(
            rd->track_p1, rd->track_p2,
            ToCol32(rd->col_border),
            rd->track_height * 0.5f
        );
    }
    // =========================
    // GRAB
    // =========================

    if (!rd->skip_draw_grab && !rd->skip_draw)
    {
        state->draw_list->AddCircleFilled(rd->grab_center, rd->grab_shadow_size, ToCol32(rd->col_grab_shadow));
        state->draw_list->AddCircleFilled(rd->grab_center, radius, ToCol32(col_grab));
        state->draw_list->AddCircle(rd->grab_center, radius,
            ToCol32(rd->col_grab_border), 0, 1.5f);
    }
}
bool CSImGui::ModernSliderFloatEx(const char* label, float* v,
    float v_min, float v_max,
    float height, float grab_radius,
    const char* format, float custom_width,
    SliderFlags flags,
    SliderRenderCallback render_cb,
    SliderTooltipCallback tooltip_cb,
    SliderSeekCallback seek_cb,
    SliderNavCallback nav_cb
)
{

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    SliderState* state = (SliderState*)window->StateStorage.GetVoidPtr(window->GetID(label));
    if (!state) {
        // Cấp phát bộ nhớ an toàn trong ImGui
        state = (SliderState*)IM_ALLOC(sizeof(SliderState));
        IM_PLACEMENT_NEW(state) SliderState();
        window->StateStorage.SetVoidPtr(window->GetID(label), state);
        state->id = window->GetID(label);
    }
    
    if (flags & SliderFlags_EnableSmoothPreview) {
        state->preview_id = ImHashData(&state->id, sizeof(ImGuiID), 0xA4158443);
        state->preview = (SliderState::SliderPreviewValue*)window->StateStorage.GetVoidPtr(state->preview_id);
        if (!state->preview) {
            // Cấp phát bộ nhớ an toàn trong ImGui
            state->preview = (SliderState::SliderPreviewValue*)IM_ALLOC(sizeof(SliderState::SliderPreviewValue));
            IM_PLACEMENT_NEW(state->preview) SliderState::SliderPreviewValue();
            window->StateStorage.SetVoidPtr(state->preview_id, state->preview);
        }
    }

    if (window->SkipItems) return false;

    state->g = GImGui;
    state->draw_list = window->DrawList;

    state->font = ImGui::GetFont();
    state->fontsize = ImGui::GetFontSize();

    // =========================
    // LABEL
    // =========================
    const char* label_end  = ImGui::FindRenderedTextEnd(label);
    if (label_end != label) // có text visible
    {
        ImGui::TextDisabled("%.*s: %s",
            (int)(label_end - label),
            label,
            TextUtils::Format(format, *v).c_str()
        );
    }

    // =========================
    // GEOMETRY
    // =========================
    state->width = (custom_width > 0) ? custom_width : ImGui::CalcItemWidth();
    state->range = (v_max - v_min);
    state->height = height;
    if (state->range <= 0.0f) state->range = 1.0f;
    state->grab_radius = grab_radius;
    state->height_max = state->grab_radius * 2.5f;
    if (state->height > state->height_max) state->height = state->height_max;
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(state->width, state->height_max);
    state->pos = window->DC.CursorPos;
    state->size = ImVec2(state->width, state->height_max);
    ImRect bb(pos, pos + size);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, state->id)) return false;

    bool is_disabled = (state->g->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    
    state->hovered = is_disabled ? false : ImGui::ItemHoverable(bb, state->id , ImGuiItemFlags_None);
    state->active = !is_disabled && (state->g->ActiveId == state->id);
    if (flags & SliderFlags_EnableSmoothPreview)
        state->display_v = state->preview->GetDisplayValue(*v);
    else
        state->display_v = *v;
    float dt = state->g->IO.DeltaTime;
    state->dt = state->g->IO.DeltaTime;
    state->v_min = v_min;
    state->v_max = v_max;
    // =========================
    // VISUAL VALUE (SMOOTH)
    // =========================
    if(flags & SliderFlags_SliderEase){
        state->anim_t = ImLerp(state->anim_t, state->t, SMOOTH_LERP(12.0f, dt));
    }else{
        state->anim_t = state->t;
    }
    state->grab_center = ImVec2(pos.x + state->anim_t * size.x, pos.y + size.y * 0.5f);
    
    // =========================
    // HOVER DETECTION (NO SQRT)
    // =========================
    state->mouse = state->g->IO.MousePos;
    float dx = state->mouse.x - state->grab_center.x;
    float dy = state->mouse.y - state->grab_center.y;

    float r = state->grab_radius * 1.5f;
    state->grab_hovered = state->hovered && (dx * dx + dy * dy <= r * r);
    state->bar_hovered  = state->hovered && !state->grab_hovered;

    // =========================
    // ACTIVE HANDLING
    // =========================
    bool changed = false;
    float new_v = state->display_v; // Mặc định là giá trị hiện tại
    bool is_interacting = false;
    bool is_click_frame = false;

    if (state->hovered && ImGui::IsMouseClicked(0))
    {
        ImGui::SetActiveID(state->id, window);
        ImGui::FocusWindow(window);
        is_click_frame = true;

        if (!(flags & SliderFlags_DisableSeek)) 
        {
            float t_curr = (*v - v_min) / state->range;
            float grab_x = pos.x + t_curr * size.x;
            float dx = state->mouse.x - grab_x;
            float dy = state->mouse.y - state->grab_center.y;
            
            // Kiểm tra xem có click vào cục Grab hay click vào thanh bar
            bool is_grab_click = (dx*dx + dy*dy <= state->grab_radius * state->grab_radius * 2.25f);

            if (is_grab_click) {
                state->g->ActiveIdClickOffset = ImVec2(dx, 0.0f); // Giữ nguyên khoảng cách tương đối với Grab
            } else if (flags & SliderFlags_EnableClickSeek) {
                state->g->ActiveIdClickOffset.x = 0.0f; // Nhảy ngay tới tâm chuột
                float t_mouse = ImClamp((state->mouse.x - pos.x) / size.x, 0.0f, 1.0f);
                new_v = v_min + t_mouse * state->range;
            }
        }
    }

    if (state->active) 
    {
        is_interacting = true;
        if (state->g->ActiveIdSource == ImGuiInputSource_Mouse) 
        {
            float mouse_x = state->mouse.x - state->g->ActiveIdClickOffset.x;
            float t = ImClamp((mouse_x - pos.x) / size.x, 0.0f, 1.0f);
            new_v = v_min + t * state->range;
        }

        if (!ImGui::IsMouseDown(0))
            ImGui::ClearActiveID();
    }
    // =========================
    // VALUE UPDATE
    // =========================

    if (is_interacting && !(flags & SliderFlags_DisableSeek))
    {
        bool is_final = !ImGui::IsMouseDown(0); // Chỉ TRUE khi thả chuột

        if (seek_cb) 
        {
            SliderSeekRequest req{};
            req.new_value  = new_v;
            req.from_click = is_click_frame;
            req.from_drag  = !is_click_frame;
            req.is_final   = is_final; 
            req.is_hovered = state->hovered;

            // Cập nhật giá trị hiển thị (Smooth Preview)
            if(state->preview) state->preview->BeginPreview(new_v);

            auto res = seek_cb(&req);
            if (res.accept) 
            {
                if (*v != res.value) {
                    *v = res.value;
                    if(state->preview) state->preview->EndPreview();
                    changed = true;
                }
            }
        } 
        else if (new_v != *v) 
        {
            // Logic mặc định nếu không có callback
            *v = new_v;
            changed = true;
        }
    }

    // ========================
    // NAV SUPPORT
    // ========================
    if (!is_disabled)
    {
        if (state->g->NavId == state->id && state->g->NavActivatePressedId == state->id)
            ImGui::SetActiveID(state->id, window);

        if (state->active && state->g->NavId == state->id && state->g->NavInputSource == ImGuiInputSource_Keyboard)
        {
            bool left  = ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
            bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow);

            if (nav_cb)
            {
                if ((left || right))
                    nav_cb(*v, left, right);
            }
            else if (!(flags & SliderFlags_NoNav))
            {
                float step = state->range * 0.01f;

                float old = *v;

                if (left)  *v -= step;
                if (right) *v += step;

                *v = ImClamp(*v, v_min, v_max);
                if (*v != old)
                    changed = true;
            }
        }
    }

    state->t = (state->display_v - v_min) / state->range;

    // =========================
    // TOOLTIP
    // =========================
    state->tooltip_id = ImHashData(&state->id, sizeof(ImGuiID), 0xA546823);
    TooltipData* td = (TooltipData*)window->StateStorage.GetVoidPtr(state->tooltip_id);
    if (!td) {
        // Cấp phát bộ nhớ an toàn trong ImGui
        td = (TooltipData*)IM_ALLOC(sizeof(TooltipData));
        IM_PLACEMENT_NEW(td) TooltipData();
        window->StateStorage.SetVoidPtr(state->tooltip_id, td);
    }
    if((flags & SliderFlags_TooltipFade) && (flags & SliderFlags_TooltipEase)){
        state->tooltip_anim_id = ImHashData(&state->tooltip_id, sizeof(ImGuiID), 0xA444682);
        td->anim = (TooltipAnimState*)window->StateStorage.GetVoidPtr(state->tooltip_anim_id);
        if(!td->anim) {
            td->anim = (TooltipAnimState*)IM_ALLOC(sizeof(TooltipAnimState));
            IM_PLACEMENT_NEW(td->anim) TooltipAnimState();
            window->StateStorage.SetVoidPtr(state->tooltip_anim_id, td->anim);
        }
    }
    // =========================
    // INIT DATA
    // =========================
    td->item.value = *v;
    td->item.v_min = v_min;
    td->item.range = state->range;
    td->config.dt = state->dt;
    td->item.pos = state->pos;
    td->item.size = state->size;
    td->item.mouse = state->mouse;
    td->item.hovered_time = state->g->HoveredIdTimer;
    td->item.active = state->active;
    td->item.hovered = state->hovered;
    td->config.font = state->font;
    td->config.fontsize = state->fontsize;
    td->item.center.y = state->grab_center.y - state->grab_radius;
    td->config.format = format;
    td->config.id = state->id;

    ToolTipFlags tdflags = ToolTipFlags_ClampItem;

    if(flags & SliderFlags_TooltipAlwaysShowAction) tdflags |= ToolTipFlags_AlwaysShowAction;
    if(flags & SliderFlags_TooltipAlwaysShow) tdflags |= ToolTipFlags_AlwaysShow;
    if(flags & SliderFlags_TooltipAnimation) tdflags |= ToolTipFlags_Animation;
    if(flags & SliderFlags_TooltipEase) tdflags |= ToolTipFlags_Ease;
    if(flags & SliderFlags_TooltipFade) tdflags |= ToolTipFlags_Fade;
    if(flags & SliderFlags_TooltipFollowMouse) tdflags |= ToolTipFlags_FollowMouse_Fixed_Y;
    if(flags & SliderFlags_TooltipHiden) tdflags |= ToolTipFlags_Hiden;
    if(flags & SliderFlags_TooltipNoArrow) tdflags |= ToolTipFlags_NoArrow;
    if(flags & SliderFlags_TooltipNoBorder) tdflags |= ToolTipFlags_NoBorder;
    if(flags & SliderFlags_TooltipScale) tdflags |= ToolTipFlags_Scale;
    
    // Tiến hành tính toán layout nâng cao và vẽ nội dung trong ToolTipEx
    ToolTipEx(td ,tdflags , tooltip_cb);


    
    state->slider_id = ImHashData(&state->id, sizeof(ImGuiID), 0xA1234567);
    SliderRenderData* rd = (SliderRenderData*)window->StateStorage.GetVoidPtr(state->slider_id );
    if(!rd){

        rd = (SliderRenderData*)IM_ALLOC(sizeof(SliderRenderData));
        IM_PLACEMENT_NEW(rd) SliderRenderData();
        window->StateStorage.SetVoidPtr(state->slider_id , rd);
    }
    if(flags & SliderFlags_SliderEase || flags & SliderFlags_SliderFade)
    {
        state->slider_anim_id  = ImHashData(&state->slider_id, sizeof(ImGuiID), 0xB7654321);
        rd->anim = (SliderRenderData::SliderAnimState*)window->StateStorage.GetVoidPtr(state->slider_anim_id);
        if (!rd->anim) {
            // Cấp phát bộ nhớ an toàn trong ImGui
            rd->anim = (SliderRenderData::SliderAnimState*)IM_ALLOC(sizeof(SliderRenderData::SliderAnimState));
            IM_PLACEMENT_NEW(rd->anim) SliderRenderData::SliderAnimState();
            window->StateStorage.SetVoidPtr(state->slider_anim_id, rd->anim);
        }
    }
    if(rd)SliderRenderBar(v, state, rd, flags, render_cb);

    return changed;
}
bool CSImGui::ToolTip(const char* label, float delay, ToolTipFlags flags) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    
    // Nếu cửa sổ cha bị thu nhỏ hoặc skip item, ta không cần xử lý tooltip nữa
    if (window->SkipItems) return false;

    const char* label_end = strstr(label, "##");
    ImGuiID id = window->GetID((label_end) ? label_end : label);
    TooltipData* state = (TooltipData*)window->StateStorage.GetVoidPtr(id);
    if (!state) {
        state = (TooltipData*)IM_ALLOC(sizeof(TooltipData));
        IM_PLACEMENT_NEW(state) TooltipData();
        window->StateStorage.SetVoidPtr(id, state);
    }

    if ((flags & ToolTipFlags_Ease) && (flags & ToolTipFlags_Fade)) {
        ImGuiID anim_id = ImHashData(&id, sizeof(ImGuiID), 0xA449682);
        state->anim = (TooltipAnimState*)window->StateStorage.GetVoidPtr(anim_id);
        if (!state->anim) {
            state->anim = (TooltipAnimState*)IM_ALLOC(sizeof(TooltipAnimState));
            IM_PLACEMENT_NEW(state->anim) TooltipAnimState();
            window->StateStorage.SetVoidPtr(anim_id, state->anim);
        }
    }

    auto& cfg = state->config;
    auto& item = state->item;


    // --- CẬP NHẬT TRẠNG THÁI TỪ CỬA SỔ CHA ---
    cfg.dt = ImGui::GetIO().DeltaTime;
    cfg.font = ImGui::GetFont();
    cfg.fontsize = ImGui::GetFontSize();
    item.mouse = ImGui::GetIO().MousePos;
    cfg.id = id;

    // Bắt tương tác dựa trên Item vừa vẽ ở cửa sổ cha
    item.hovered = ImGui::IsItemHovered();
    item.active = ImGui::IsItemActive();
    item.pos = ImGui::GetItemRectMin();
    item.size = ImGui::GetItemRectSize();
    item.center = item.pos + item.size * 0.5f;
    cfg.max_width = 500.0f;
    cfg.max_height = 500.0f;
    cfg.title_max_lines = 5;

    if (item.hovered) item.hovered_time += cfg.dt;
    else item.hovered_time = 0;
    item.hover_delay = delay;

    if (label_end)
        cfg.title = Format("%.*s", (int)(label_end - label), label);
    else 
        cfg.title = label;

    cfg.show_title = !cfg.title.empty();
    cfg.show_text = false;

    item.cursor_size = ImGui::GetIO().MouseDrawCursor
        ? ImVec2(16, 16)
        : ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow
            ? ImVec2(16, 16)
            : ImVec2(20, 20);


    // --- KHỞI TẠO CỬA SỔ TOOLTIP RIÊNG BIỆT ---

    ToolTipFlags flags_ex = ToolTipFlags_FollowMouse | ToolTipFlags_AutoPosition | flags;
    
    // Tiến hành tính toán layout nâng cao và vẽ nội dung trong ToolTipEx
    ToolTipEx(state, flags_ex);



    return true;
}

bool CSImGui::ModernSliderFloat(const char* label, float* v,
                         float v_min, float v_max, 
                        float height, float grab_radius, 
                        const char* format, float custom_width,
                        SliderFlags flag ) {
                            
    ImGui::PushStyleColor(ImGuiCol_PopupBg              ,GetColors(Col_PopupBg));
    ImGui::PushStyleColor(ImGuiCol_Border               ,GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Text                 ,GetColors(Col_Text));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered       ,GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBg              ,GetColors(Col_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram        ,GetColors(Col_PlotHistogram));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogramActive  ,GetColors(Col_PlotHistogramActive));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabHovered    ,GetColors(Col_SliderGrabHovered));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive     ,GetColors(Col_SliderGrabActive));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab           ,GetColors(Col_SliderGrab));
    ImGui::PushStyleColor(ImGuiCol_CheckMark            ,GetColors(Col_CheckMark));
    SliderFlags flag_ex = SliderFlags_Default|
                        SliderFlags_Animation|
                        flag;
    bool s = ModernSliderFloatEx(label,v,v_min,v_max, 
                        height, grab_radius, 
                        format,custom_width,
                        flag_ex);
                 
    ImGui::PopStyleColor(11);
    return s;

}

bool CSImGui::ModernInputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
    // 1. Setup Style tương tự Multiline nhưng padding dọc nhỏ hơn để cân đối
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8)); 
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    // 2. Màu sắc từ Theme
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          GetColors(Col_FrameBg)); 
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   GetColors(Col_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    GetColors(Col_FrameBgActive));
    ImGui::PushStyleColor(ImGuiCol_Border,           GetColors(Col_Border)); 
    ImGui::PushStyleColor(ImGuiCol_Text,             GetColors(Col_Text));

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
bool CSImGui::ModernSmallButton(const char* label) {
    // Nút nhỏ cần bo góc ít hơn một chút hoặc giữ nguyên để đồng bộ
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2)); // Padding cực nhỏ cho Small Button
    
    // Sử dụng màu của SecondaryButton hoặc một màu Neutral hơn
    ImGui::PushStyleColor(ImGuiCol_Button,          GetColors(Col_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,   GetColors(Col_ButtonHovered)); // Hover vẫn cho màu chính
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,    GetColors(Col_ButtonActive));
    ImGui::PushStyleColor(ImGuiCol_Text,            GetColors(Col_Text));

    bool pressed = ImGui::Button(label);

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    return pressed;
}
bool CSImGui::ModernArrowButton(const char* str_id, ImGuiDir dir, ImVec2 size) {
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
    ImGui::PushStyleColor(ImGuiCol_Button,        GetColors(Col_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GetColors(Col_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  GetColors(Col_ButtonActive));
    ImGui::PushStyleColor(ImGuiCol_Text,           GetColors(Col_Text));

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
void CSImGui::ShowTooltipDelayed(const char* text, bool hovering, double delaySeconds)
{
    ImGuiID base_id  = ImGui::GetID(text);

    ImGuiID delay_id = ImHashStr("delay", 0, base_id);
    ImGuiID alpha_id = ImHashStr("alpha", 0, base_id);

    bool shouldShow = SetDelayHover(hovering, delaySeconds, delay_id);

    float* pAlpha = ImGui::GetStateStorage()->GetFloatRef(alpha_id, 0.0f);

    float fadeSpeed = 12.0f;
    UpdateHoverAnim(*pAlpha, shouldShow, fadeSpeed);

    if (*pAlpha <= 0.001f)
        return;

    ImGui::PushStyleColor(ImGuiCol_PopupBg, GetColors(Col_PopupBg));
    ImGui::PushStyleColor(ImGuiCol_Border,  GetColors(Col_Border));
    ImGui::PushStyleColor(ImGuiCol_Text,    GetColors(Col_Text));

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, *pAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGui::SetNextWindowBgAlpha(*pAlpha);

    if (ImGui::BeginTooltip())
    {
        ImGui::TextUnformatted(text);
        ImGui::EndTooltip();
    }

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(3);
}
bool CSImGui::ModernToggle(const char* str_id, bool* v, bool enabled, float scale) {
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
    ImVec4 col_off = GetColors(Col_FrameBg);
    ImVec4 col_on  = GetColors(Col_CheckMark);
    
    // Nội suy màu nền
    ImVec4 current_col = ImLerp(col_off, col_on, *pT);
    
    // Nếu disabled, giảm độ đậm (Alpha) của cả background và knob
    float final_alpha = enabled ? 1.0f : 0.35f; 
    ImU32 bg_color = ImGui::ColorConvertFloat4ToU32(ImVec4(current_col.x, current_col.y, current_col.z, final_alpha));
    ImU32 knob_color = ImGui::ColorConvertFloat4ToU32(ImVec4(GetColors(Col_Text).x, GetColors(Col_Text).y, GetColors(Col_Text).z, final_alpha));

    // 4. Vẽ Background & Knob
    window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_color, 10.0f);
    
    float knob_pos_x = bb.Min.x + radius + (*pT * (width - height));
    window->DrawList->AddCircleFilled(ImVec2(knob_pos_x, bb.Min.y + radius), radius - 2.0f, knob_color);

    return pressed;
}
void CSImGui::ModernHeader(const char* title, float scale) {
    ImGui::Spacing();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float height = ImGui::GetFontSize() + (4.0f * scale);

    // Vẽ thanh chỉ báo dọc (Indicator bar)
    draw_list->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + 3.0f * scale, p.y + height), ToCol32(GetColors(Col_CheckMark)), 2.0f);

    // Vẽ Title
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_Text, GetColors(Col_Text));
    ImGui::Text(title);
    ImGui::PopStyleColor();

    // Separator mờ dần hoặc màu mỏng
    ImVec4 sep_col = GetColors(Col_Separator);
    sep_col.w = 0.3f; // Giảm độ đậm của gạch ngang
    ImGui::PushStyleColor(ImGuiCol_Separator, sep_col);
    ImGui::Separator();
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
}
void CSImGui::DrawCardWithHole(ImDrawList* dl,const ImVec2& cardMin,
    const ImVec2& cardMax,const ImVec2& holeMin,const ImVec2& holeMax,
    ImU32 fillCol,ImU32 borderCol,const CardHoleStyle& style) {
    const float r = style.rounding;
    // =========================
    // FILL
    // =========================
    // Phải
    dl->PushClipRect(ImVec2(holeMax.x, cardMin.y),cardMax,true);
    dl->AddRectFilled(cardMin,cardMax,fillCol,r, ImDrawFlags_RoundCornersRight);
    dl->PopClipRect();
    // Trên trái
    if (holeMin.y > cardMin.y){
        dl->PushClipRect(cardMin,ImVec2(holeMax.x, holeMin.y),true);
        dl->AddRectFilled(cardMin,cardMax,fillCol,r,ImDrawFlags_RoundCornersTopLeft);
        dl->PopClipRect();
    }
    // ⭐ LEFT – MIDDLE (BỔ SUNG)
    dl->PushClipRect(ImVec2(cardMin.x, holeMin.y),ImVec2(holeMin.x, holeMax.y),true);
    dl->AddRectFilled(cardMin,cardMax,fillCol,0.0f,ImDrawFlags_None);
    dl->PopClipRect();
    // Dưới trái
    if (holeMax.y < cardMax.y){
        dl->PushClipRect(ImVec2(cardMin.x, holeMax.y),ImVec2(holeMax.x, cardMax.y),true);
        dl->AddRectFilled(cardMin,cardMax,fillCol,r,ImDrawFlags_RoundCornersBottomLeft);
        dl->PopClipRect();
    }
    // =========================
    // BORDER
    // =========================
    if ((borderCol >> IM_COL32_A_SHIFT) > 0){
        dl->PushClipRect(ImVec2(holeMax.x, cardMin.y),cardMax,true);
        dl->AddRect(cardMin,cardMax,borderCol,r,ImDrawFlags_RoundCornersRight,style.borderThickness);
        dl->PopClipRect();
        // Trên trái
        if (holeMin.y > cardMin.y){
            dl->PushClipRect(cardMin,ImVec2(holeMax.x, holeMin.y),true);
            dl->AddRect(cardMin,cardMax,borderCol,r,ImDrawFlags_RoundCornersTopLeft,style.borderThickness);
            dl->PopClipRect();
        }
        // ⭐ LEFT – MIDDLE (BỔ SUNG)
        dl->PushClipRect(ImVec2(cardMin.x, holeMin.y),ImVec2(holeMin.x, holeMax.y),true);
        dl->AddRect(cardMin,cardMax,borderCol,0.0f, ImDrawFlags_None,style.borderThickness);
        dl->PopClipRect();
        // Dưới trái
        if (holeMax.y < cardMax.y){
            dl->PushClipRect(ImVec2(cardMin.x, holeMax.y),ImVec2(holeMax.x, cardMax.y),true);
            dl->AddRect(cardMin,cardMax,borderCol,r,ImDrawFlags_RoundCornersBottomLeft,style.borderThickness);
            dl->PopClipRect();
        }
    }
}
