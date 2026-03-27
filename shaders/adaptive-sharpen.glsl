//------------------------------------------------------------------
// MPV GLSL Shader: Smart Adaptive Sharpen (Balanced)
// Mục tiêu: Làm nét tự nhiên, không halo, dùng xem video bình thường
//------------------------------------------------------------------

//!HOOK MAIN
//!BIND HOOKED
//!DESC Smart Adaptive Sharpen (Natural)

// 🔧 Tuning
#define SHARPEN_STRENGTH 0.9
#define EDGE_LOW  0.02
#define EDGE_HIGH 0.12
#define CLAMP_LIMIT 0.04

vec4 hook() {
    vec4 c = HOOKED_texOff(0);

    vec2 px = HOOKED_pt;

    vec4 t = HOOKED_texOff(vec2(0,-1)*px);
    vec4 b = HOOKED_texOff(vec2(0, 1)*px);
    vec4 l = HOOKED_texOff(vec2(-1,0)*px);
    vec4 r = HOOKED_texOff(vec2( 1,0)*px);

    // 🔹 Laplacian edge
    vec4 edge = 4.0*c - (t + b + l + r);

    // 🔹 Edge strength (độ mạnh cạnh)
    float e = length(edge.rgb);

    // 🔹 Adaptive factor (chỉ sharpen vùng có cạnh)
    float factor = smoothstep(EDGE_LOW, EDGE_HIGH, e);

    // 🔹 Sharpen cơ bản
    vec4 sharp = c + edge * SHARPEN_STRENGTH * factor;

    // 🔥 Anti-halo clamp (quan trọng)
    vec4 minN = min(min(t, b), min(l, r));
    vec4 maxN = max(max(t, b), max(l, r));

    sharp = clamp(sharp, minN - CLAMP_LIMIT, maxN + CLAMP_LIMIT);

    return clamp(sharp, 0.0, 1.0);
}