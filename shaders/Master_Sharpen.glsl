/*
@NAME: Ultra_Detail_V12_Stable
@DESC: Stable Edge Reconstruction + Adaptive CAS (90% madVR, ít lỗi)
@HOOK: MAIN
@BIND: HOOKED

@PARAM: DETAIL_STRENGTH
@LABEL: Độ nét
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.5
@INFO: Điều khiển độ sharpen (CAS).
Giá trị cao → chi tiết rõ hơn nhưng dễ:
- tạo halo (viền sáng)
- rung viền (khi có chuyển động)
Giá trị thấp → mềm hơn, ít artifact.
Khuyên dùng:
0.45–0.6 (cân bằng),
>0.65 (gaming / nét mạnh),
<0.4 (phim / tự nhiên).

@PARAM: EDGE_RECON
@LABEL: Tái tạo cạnh
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.55
@INFO: Mức độ "vẽ lại" cạnh (edge reconstruction).
Tăng giá trị → viền mượt hơn, giảm răng cưa tốt hơn.
Nhưng nếu quá cao:
- có thể lệch màu
- làm viền bị "trôi" nhẹ (ghosting)
Thấp → giữ nguyên pixel gốc nhiều hơn.
Khuyên dùng:
0.5–0.6 (ổn định),
>0.65 (anime line-art),
<0.45 (video noise / live).

@PARAM: STABILITY
@LABEL: Độ ổn định
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.6
@INFO: Kiểm soát chống rung, flicker và giữ texture gốc.
Cao → ổn định hơn, ít rung nhưng hơi mềm.
Thấp → nét hơn nhưng dễ:
- flicker (nhất là với temporal/bloom)
- noise bị khuếch đại
Khuyên dùng:
0.55–0.75 (video / phim),
0.4–0.55 (gaming / nét cao).
*/

#define LUMA vec3(0.2126, 0.7152, 0.0722)

vec4 hook() {
    vec2 pos = HOOKED_pos;
    vec2 px = HOOKED_pt;

    // ===== SAMPLE =====
    vec3 c = HOOKED_tex(pos).rgb;
    vec3 n = HOOKED_tex(pos + vec2(0.0, -px.y)).rgb;
    vec3 s = HOOKED_tex(pos + vec2(0.0, px.y)).rgb;
    vec3 w = HOOKED_tex(pos + vec2(-px.x, 0.0)).rgb;
    vec3 e = HOOKED_tex(pos + vec2(px.x, 0.0)).rgb;

    // ===== LUMA =====
    float lc = dot(c, LUMA);
    float ln = dot(n, LUMA), ls = dot(s, LUMA);
    float lw = dot(w, LUMA), le = dot(e, LUMA);

    float lMin = min(lc, min(min(ln, ls), min(lw, le)));
    float lMax = max(lc, max(max(ln, ls), max(lw, le)));
    float contrast = lMax - lMin;

    if (contrast < 0.01)
        return vec4(c, 1.0);

    // ===== STABLE EDGE DETECTION =====
    float gx = le - lw;
    float gy = ls - ln;

    float len = max(abs(gx) + abs(gy), 1e-4);
    vec2 dir = vec2(gx, gy) / len;

    // ===== EDGE CONFIDENCE =====
    float edge = sqrt(gx*gx + gy*gy) / (contrast + 1e-5);
    float confidence = smoothstep(0.1, 0.6, edge);

    // ===== LIMITED RECONSTRUCTION =====
    vec3 fwd = HOOKED_tex(pos + dir * px).rgb;
    vec3 bwd = HOOKED_tex(pos - dir * px).rgb;

    vec3 recon = (fwd + bwd) * 0.5;

    // fallback khi edge yếu
    vec3 avg4 = (n + s + w + e) * 0.25;

    recon = mix(avg4, recon, confidence);
    // ===== ADAPTIVE FACTORS =====

    // độ mạnh cạnh
    float edgeFactor = smoothstep(0.05, 0.4, contrast);

    // độ phức tạp texture
    float textureVar =
        abs(lc-ln)+abs(lc-ls)+abs(lc-lw)+abs(lc-le);

    float textureFactor = smoothstep(0.02, 0.15, textureVar);

    // ===== AUTO PARAM =====
    float autoDetail = mix(DETAIL_STRENGTH * 0.6, DETAIL_STRENGTH, edgeFactor);
    float autoRecon  = mix(EDGE_RECON * 0.5, EDGE_RECON, edgeFactor);
    float autoStable = mix(STABILITY, STABILITY * 0.5, textureFactor);

    // ===== APPLY EDGE =====
    vec3 edgeColor = mix(c, recon, autoRecon * confidence);

    // ===== STABILITY FILTER =====
    float variance =
        abs(lc-ln)+abs(lc-ls)+abs(lc-lw)+abs(lc-le);

    float stable = smoothstep(0.02, 0.2, variance);

    edgeColor = mix(edgeColor, c, autoStable * stable);

    // ===== SAFE CAS =====
    float amp = clamp(min(lMin, 1.0 - lMax)/(lMax+1e-5),0.0,1.0);
    float wCAS = -amp * mix(0.25, 0.06, autoDetail);

    vec3 sharpened = (
        n*wCAS + s*wCAS + w*wCAS + e*wCAS + edgeColor
    ) / (1.0 + 4.0*wCAS);

    // ===== CLAMP =====
    vec3 lo = min(min(n,s),min(w,e));
    vec3 hi = max(max(n,s),max(w,e));

    vec3 finalColor = clamp(sharpened, min(lo,c), max(hi,c));

    return vec4(finalColor, 1.0);
}