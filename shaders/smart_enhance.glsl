//------------------------------------------------------------------
// MPV GLSL Shader: Smart Enhance (Upscale + Sharpen + Color)
// Mục tiêu: Tăng nét + cải thiện màu tự nhiên, không bị gắt
//------------------------------------------------------------------

//!HOOK OUTPUT
//!BIND HOOKED
//!DESC Smart Enhance (Upscale + Sharpen + Natural Color)

// ====== TUNING ======
#define SHARPEN_STRENGTH 0.8
#define EDGE_LOW  0.02
#define EDGE_HIGH 0.10
#define CLAMP_LIMIT 0.04

#define SATURATION 1.05
#define CONTRAST   1.03
#define BRIGHTNESS 0.00

#define UPSCALE_SOFTNESS 0.2  // càng thấp càng nét

vec3 applyColor(vec3 c) {
    // Contrast
    c = (c - 0.5) * CONTRAST + 0.5;

    // Brightness
    c += BRIGHTNESS;

    // Saturation
    float luma = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(luma), c, SATURATION);

    return clamp(c, 0.0, 1.0);
}

vec4 hook() {
    vec2 px = HOOKED_pt;

    // ====== SAMPLE 3x3 (giả upscale mềm) ======
    vec4 c  = HOOKED_texOff(0);
    vec4 t  = HOOKED_texOff(vec2(0,-1)*px);
    vec4 b  = HOOKED_texOff(vec2(0, 1)*px);
    vec4 l  = HOOKED_texOff(vec2(-1,0)*px);
    vec4 r  = HOOKED_texOff(vec2( 1,0)*px);

    vec4 tl = HOOKED_texOff(vec2(-1,-1)*px);
    vec4 tr = HOOKED_texOff(vec2( 1,-1)*px);
    vec4 bl = HOOKED_texOff(vec2(-1, 1)*px);
    vec4 br = HOOKED_texOff(vec2( 1, 1)*px);

    // ====== Upscale (lọc mềm + edge-aware) ======
    vec4 blur = (t + b + l + r + tl + tr + bl + br) / 8.0;
    vec4 upscale = mix(blur, c, UPSCALE_SOFTNESS);

    // ====== Edge detect ======
    vec4 edge = 4.0*c - (t + b + l + r);
    float e = length(edge.rgb);

    float factor = smoothstep(EDGE_LOW, EDGE_HIGH, e);

    // ====== Sharpen ======
    vec4 sharp = upscale + edge * SHARPEN_STRENGTH * factor;

    // ====== Anti-halo ======
    vec4 minN = min(min(t, b), min(l, r));
    vec4 maxN = max(max(t, b), max(l, r));
    sharp = clamp(sharp, minN - CLAMP_LIMIT, maxN + CLAMP_LIMIT);

    // ====== Color correction ======
    vec3 finalColor = applyColor(sharp.rgb);

    return vec4(finalColor, c.a);
}