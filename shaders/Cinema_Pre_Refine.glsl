/*
@NAME: Cinema_Pre_Refine_V1
@DESC: Khử nhiễu (Denoise) + Làm mượt khối (Smoothing) cho ảnh thấp
@HOOK: PREKERNEL
@BIND: HOOKED

@PARAM: DENOISE_STR
@LABEL: Mức độ khử nhiễu
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.45
@INFO: Khử các hạt nhiễu (grain) và vết rỗ.
Cao -> Ảnh mượt như tranh vẽ nhưng mất texture thật.
Thấp -> Giữ lại chi tiết gốc.

@PARAM: SMART_SMOOTH
@LABEL: Làm mượt vùng phẳng
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.3
@INFO: Giúp các vùng da, bầu trời mượt hơn mà không làm mờ cạnh.
*/

#define LUMA vec3(0.2126, 0.7152, 0.0722)

vec4 hook() {
    vec2 pos = HOOKED_pos;
    vec2 px = HOOKED_pt;
    
    vec3 c = HOOKED_tex(pos).rgb;
    
    // Thuật toán Median lọc nhiễu nhẹ
    vec3 n = HOOKED_tex(pos + vec2(0, -px.y)).rgb;
    vec3 s = HOOKED_tex(pos + vec2(0, px.y)).rgb;
    vec3 w = HOOKED_tex(pos + vec2(-px.x, 0)).rgb;
    vec3 e = HOOKED_tex(pos + vec2(px.x, 0)).rgb;
    
    vec3 avg = (n + s + w + e + c) * 0.2;
    
    // Tính toán độ lệch (variance) để nhận diện vùng nhiễu
    float diff = distance(c, avg);
    float edgeMask = smoothstep(0.02, 0.1, diff); // 1.0 là cạnh, 0.0 là vùng phẳng nhiễu
    
    // Khử nhiễu: Áp dụng mạnh vào vùng phẳng (edgeMask thấp), nhẹ vào vùng cạnh
    vec3 denoised = mix(avg, c, edgeMask * (1.0 - DENOISE_STR * 0.5));
    
    // Smart Smooth: Giảm bớt các vết rỗ pixel
    vec3 result = mix(c, denoised, DENOISE_STR);
    
    return vec4(result, 1.0);
}