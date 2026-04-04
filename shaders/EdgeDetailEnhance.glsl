/*
@NAME: Edge Detail Enhance
@DESC: Tăng viền chi tiết nhẹ, không bị gắt
@HOOK: MAIN
@ORDER: 320

@PARAM: STRENGTH
@LABEL: Cường độ viền
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 0.8

@PARAM: THICKNESS
@LABEL: Độ dày viền
@MIN: 1.0
@MAX: 3.0
@DEFAULT: 1.0

@PARAM: THRESHOLD
@LABEL: Ngưỡng phát hiện viền
@MIN: 0.0
@MAX: 0.2
@DEFAULT: 0.05
*/

vec4 hook() {
    vec2 px = HOOKED_pt * THICKNESS;

    vec3 c = HOOKED_tex(HOOKED_pos).rgb;

    // Sobel edge detection
    float gx =
        -1.0 * dot(HOOKED_tex(HOOKED_pos + vec2(-px.x, -px.y)).rgb, vec3(0.299,0.587,0.114)) +
         1.0 * dot(HOOKED_tex(HOOKED_pos + vec2( px.x, -px.y)).rgb, vec3(0.299,0.587,0.114)) +
        -2.0 * dot(HOOKED_tex(HOOKED_pos + vec2(-px.x,  0)).rgb, vec3(0.299,0.587,0.114)) +
         2.0 * dot(HOOKED_tex(HOOKED_pos + vec2( px.x,  0)).rgb, vec3(0.299,0.587,0.114)) +
        -1.0 * dot(HOOKED_tex(HOOKED_pos + vec2(-px.x,  px.y)).rgb, vec3(0.299,0.587,0.114)) +
         1.0 * dot(HOOKED_tex(HOOKED_pos + vec2( px.x,  px.y)).rgb, vec3(0.299,0.587,0.114));

    float gy =
        -1.0 * dot(HOOKED_tex(HOOKED_pos + vec2(-px.x, -px.y)).rgb, vec3(0.299,0.587,0.114)) +
        -2.0 * dot(HOOKED_tex(HOOKED_pos + vec2( 0,    -px.y)).rgb, vec3(0.299,0.587,0.114)) +
        -1.0 * dot(HOOKED_tex(HOOKED_pos + vec2( px.x, -px.y)).rgb, vec3(0.299,0.587,0.114)) +
         1.0 * dot(HOOKED_tex(HOOKED_pos + vec2(-px.x,  px.y)).rgb, vec3(0.299,0.587,0.114)) +
         2.0 * dot(HOOKED_tex(HOOKED_pos + vec2( 0,     px.y)).rgb, vec3(0.299,0.587,0.114)) +
         1.0 * dot(HOOKED_tex(HOOKED_pos + vec2( px.x,  px.y)).rgb, vec3(0.299,0.587,0.114));

    float edge = sqrt(gx * gx + gy * gy);

    // Threshold mềm
    float mask = smoothstep(THRESHOLD, THRESHOLD * 2.0, edge);

    // Viền sáng nhẹ (có thể đổi sang viền tối nếu thích)
    vec3 edgeColor = vec3(edge);

    vec3 result = c + edgeColor * STRENGTH * mask;

    return vec4(result, 1.0);
}