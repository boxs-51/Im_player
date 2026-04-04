/*
@NAME: Fine Detail Sharpen
@DESC: Làm nét chi tiết nhỏ, tránh halo vùng lớn
@HOOK: MAIN
@ORDER: 310

@PARAM: STRENGTH
@LABEL: Độ sắc nét
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 1.0

@PARAM: THRESHOLD
@LABEL: Ngưỡng chi tiết nhỏ
@MIN: 0.0
@MAX: 0.1
@DEFAULT: 0.02
*/

vec4 hook() {
    vec2 px = HOOKED_pt;

    vec3 center = HOOKED_tex(HOOKED_pos).rgb;

    // Blur nhẹ (low-pass)
    vec3 blur =
        HOOKED_tex(HOOKED_pos + vec2(px.x, 0)).rgb +
        HOOKED_tex(HOOKED_pos - vec2(px.x, 0)).rgb +
        HOOKED_tex(HOOKED_pos + vec2(0, px.y)).rgb +
        HOOKED_tex(HOOKED_pos - vec2(0, px.y)).rgb;

    blur *= 0.25;

    // High-pass (chi tiết nhỏ)
    vec3 detail = center - blur;

    // Lấy magnitude của detail
    float mag = length(detail);

    // Chỉ sharpen khi là chi tiết nhỏ
    float mask = smoothstep(THRESHOLD, THRESHOLD * 2.0, mag);

    vec3 result = center + detail * STRENGTH * mask;

    return vec4(result, 1.0);
}