/*
@NAME: Cinema_Luma_Refine_V1
@DESC: Khử nhiễu vùng tối, chống Ringing (halo) và ổn định màu sắc
@HOOK: MAIN
@BIND: HOOKED

@PARAM: DE_RINGING
@LABEL: Khử viền ảo (Halo)
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.4
@INFO: Giảm các viền trắng/đen quá gắt do sharpen gây ra.
Khuyên dùng: 0.35 - 0.5.

@PARAM: NOISE_CLEAN
@LABEL: Làm sạch nhiễu
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.25
@INFO: Khử các hạt nhiễu li ti trong vùng tối để Bloom trông sạch hơn.
*/

#define LUMA vec3(0.2126, 0.7152, 0.0722)

vec4 hook() {
    vec2 pos = HOOKED_pos;
    vec2 px = HOOKED_pt;
    
    // Sample 5 điểm (Cross)
    vec3 c = HOOKED_tex(pos).rgb;
    vec3 n = HOOKED_tex(pos + vec2(0.0, -px.y)).rgb;
    vec3 s = HOOKED_tex(pos + vec2(0.0, px.y)).rgb;
    vec3 w = HOOKED_tex(pos + vec2(-px.x, 0.0)).rgb;
    vec3 e = HOOKED_tex(pos + vec2(px.x, 0.0)).rgb;

    // Tính toán giới hạn (Min/Max clamping) để khử Ringing
    vec3 lp = min(min(n, s), min(w, e));
    vec3 hp = max(max(n, s), max(w, e));
    
    // Thuật toán De-ringing: Giới hạn màu pixel trung tâm vào vùng lân cận
    vec3 deRinged = clamp(c, lp, hp);
    vec3 result = mix(c, deRinged, DE_RINGING);

    // Luma-based Noise Cleaning (Chỉ xử lý vùng tối/trung bình)
    float luma = dot(result, LUMA);
    float noiseMask = smoothstep(0.4, 0.0, luma); // Tập trung vào vùng tối
    
    vec3 avg = (n + s + w + e) * 0.25;
    result = mix(result, mix(result, avg, 0.5), noiseMask * NOISE_CLEAN);

    return vec4(result, 1.0);
}