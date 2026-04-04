/*
@NAME: Sharpen Pro
@DESC: Làm nét thông minh, hạn chế halo
@HOOK: MAIN

@PARAM: STRENGTH
@LABEL: Độ sắc nét
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 1.0
*/

vec4 hook() {
    vec2 px = HOOKED_pt;

    vec3 c = HOOKED_tex(HOOKED_pos).rgb;

    vec3 blur =
        HOOKED_tex(HOOKED_pos + vec2(px.x, 0)).rgb +
        HOOKED_tex(HOOKED_pos - vec2(px.x, 0)).rgb +
        HOOKED_tex(HOOKED_pos + vec2(0, px.y)).rgb +
        HOOKED_tex(HOOKED_pos - vec2(0, px.y)).rgb;

    blur *= 0.25;

    vec3 detail = c - blur;

    return vec4(c + detail * STRENGTH, 1.0);
}