/*
@NAME: Vibrance Pro
@DESC: Tăng màu thông minh (giữ tone da)
@HOOK: OUTPUT

@PARAM: AMOUNT
@LABEL: Cường độ màu
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 0.7
*/

vec4 hook() {
    vec3 color = HOOKED_tex(HOOKED_pos).rgb;

    float avg = (color.r + color.g + color.b) / 3.0;
    float mx = max(max(color.r, color.g), color.b);

    float factor = (mx - avg) * AMOUNT;

    color = mix(vec3(avg), color, 1.0 + factor);

    return vec4(color, 1.0);
}