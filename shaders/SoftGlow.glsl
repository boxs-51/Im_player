/*
@NAME: Soft Glow
@DESC: Glow nhẹ cho highlight
@HOOK: OUTPUT

@PARAM: STRENGTH
@LABEL: Độ glow
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.25
*/

vec4 hook() {
    vec2 px = HOOKED_pt;

    vec3 blur =
        HOOKED_tex(HOOKED_pos + px).rgb +
        HOOKED_tex(HOOKED_pos - px).rgb +
        HOOKED_tex(HOOKED_pos + vec2(px.x, -px.y)).rgb +
        HOOKED_tex(HOOKED_pos + vec2(-px.x, px.y)).rgb;

    blur *= 0.25;

    vec3 base = HOOKED_tex(HOOKED_pos).rgb;

    return vec4(base + blur * STRENGTH, 1.0);
}