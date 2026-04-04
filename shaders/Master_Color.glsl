/*
@NAME: Cinema_Master_Color_V5
@DESC: Smart Color Grading (zone-based + skin protection)
@HOOK: OUTPUT

@PARAM: EXPOSURE
@LABEL: Độ sáng tổng
@MIN: 0.5
@MAX: 2.0
@DEFAULT: 1.05

@PARAM: HIGHLIGHT_FIX
@LABEL: Khử cháy sáng
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.5

@PARAM: VIBRANCE
@LABEL: Độ tươi màu thông minh
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 1.12
*/

#define LUMA vec3(0.2126, 0.7152, 0.0722)


// ===== TONE MAP =====
vec3 tone_map(vec3 x, float t) {
    return x / (x + t * x * x + 0.0001) * (1.0 + t);
}


// ===== SKIN DETECTION (approx) =====
float skinMask(vec3 c){
    float r = c.r;
    float g = c.g;
    float b = c.b;

    float cond =
        step(0.35, r) *
        step(g, r) *
        step(b, g) *
        step(0.1, r - b);

    return cond;
}


// ===== SMART VIBRANCE =====
vec3 smartVibrance(vec3 col, float strength){
    float l = dot(col, LUMA);

    float max_c = max(col.r, max(col.g, col.b));
    float min_c = min(col.r, min(col.g, col.b));
    float sat = max_c - min_c;

    // ===== ZONE WEIGHTS =====
    float shadowW    = smoothstep(0.0, 0.4, l);
    float midW       = smoothstep(0.2, 0.7, l) * (1.0 - smoothstep(0.6, 1.0, l));
    float highlightW = smoothstep(0.6, 1.0, l);

    float vib =
        shadowW * 0.25 +
        midW * 0. +
        highlightW * 0.35;

    // giảm nếu màu đã đậm
    vib *= (1.0 - smoothstep(0.4, 1.0, sat));

    // ===== SKIN PROTECTION =====
    float skin = skinMask(col);
    vib *= mix(1.0, 0.4, skin);

    // ===== APPLY =====
    vec3 gray = vec3(l);
    return mix(gray, col, 1.0 + (strength - 1.0) * vib);
}


vec4 hook() {
    vec3 col = HOOKED_tex(HOOKED_pos).rgb;

    float l0 = dot(col, LUMA);

    // ===== 1. ADAPTIVE EXPOSURE =====
    float lift = clamp(1.0 - l0, 0.0, 1.0);
    col *= (1.0 + lift * (EXPOSURE - 1.0));

    // ===== 2. HIGHLIGHT RECOVERY (smooth hơn) =====
    float hMask = smoothstep(0.7, 1.0, l0);
    col = mix(col, tone_map(col, HIGHLIGHT_FIX), hMask);

    // ===== 3. SMART VIBRANCE =====
    col = smartVibrance(col, VIBRANCE);

    // ===== 4. FINAL CLAMP =====
    col = clamp(col, 0.0, 1.0);

    return vec4(col, 1.0);
}