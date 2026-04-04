/*
@NAME: Filmic ToneMap
@DESC: Tone mapping kiểu phim điện ảnh
@HOOK: LINEAR

@PARAM: EXPOSURE
@LABEL: Phơi sáng
@MIN: 0.5
@MAX: 2.0
@DEFAULT: 1.0
*/

vec4 hook() {
    vec3 color = HOOKED_tex(HOOKED_pos).rgb;

    color *= EXPOSURE;

    // Filmic curve
    color = color / (color + vec3(1.0));

    return vec4(color, 1.0);
}