/*
@NAME: Bloom_Cinema_Pro_V4_CinemaLens
@DESC: Multi-scale + Anamorphic + Lens Dirt + Filmic Bloom
@HOOK: LINEAR
@BIND: PREKERNEL

@PARAM: THRESHOLD
@LABEL: Ngưỡng sáng
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.7
@INFO: Xác định pixel nào được coi là "highlight" để tạo bloom.
Giá trị cao → chỉ vùng rất sáng mới glow (chuẩn phim).
Giá trị thấp → nhiều vùng sáng vừa cũng glow (dễ bị wash).
Khuyên dùng: 0.65–0.75.

@PARAM: STRENGTH
@LABEL: Cường độ Bloom
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 0.35
@INFO: Độ mạnh tổng thể của hiệu ứng bloom.
Cao → glow rõ, cinematic nhưng dễ cháy sáng.
Thấp → subtle, tự nhiên.
Khuyên dùng: 0.3–0.5 (cinematic), >0.6 (neon/stylized).

@PARAM: RANGE
@LABEL: Độ lan
@MIN: 1.0
@MAX: 5.0
@DEFAULT: 2.5
@INFO: Bán kính lan của bloom (độ rộng glow).
Thấp → glow nhỏ, sắc.
Cao → glow rộng, mềm, tạo halo lớn.
Ảnh hưởng trực tiếp tới performance.
Khuyên dùng: 2.0–3.0.

@PARAM: ANAMORPHIC
@LABEL: Kéo ngang (cinema lens)
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 1.2
@INFO: Giả lập ống kính anamorphic (kéo bloom theo chiều ngang).
=1.0 → không kéo (bloom tròn).
>1.0 → bloom kéo ngang (phong cách điện ảnh Hollywood).
>1.5 → hiệu ứng rõ, phù hợp cảnh đêm / ánh đèn.
Khuyên dùng: 1.2–1.4.

@PARAM: DIRT
@LABEL: Lens dirt
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.3
@INFO: Mô phỏng bụi/nhòe trên ống kính làm ánh sáng loang không đều.
Tăng giá trị → bloom có texture, cảm giác "quay phim thật".
Quá cao → bẩn, fake.
Khuyên dùng: 0.2–0.4.
*/

#define LUMA vec3(0.2126,0.7152,0.0722)

float bright(vec3 c){
    return smoothstep(THRESHOLD, THRESHOLD+0.25, dot(c,LUMA));
}

vec3 sampleBloom(vec2 pos, vec2 offset){
    vec3 c = HOOKED_tex(pos + offset).rgb;
    return c * bright(c);
}

vec4 hook(){
    vec2 pos = HOOKED_pos;
    vec2 px = HOOKED_pt;

    vec3 base = HOOKED_tex(pos).rgb;

    // ===== SCALE =====
    vec2 r1 = px * RANGE;
    vec2 r2 = px * RANGE * 2.0;
    vec2 r3 = px * RANGE * 4.0;

    // ===== ANAMORPHIC SCALE (horizontal stretch) =====
    vec2 ax = vec2(px.x * ANAMORPHIC, px.y);

    // ===== NEAR =====
    vec3 near =
        sampleBloom(pos, vec2(0,0))*0.3 +
        sampleBloom(pos, vec2(ax.x,0))*0.175 +
        sampleBloom(pos, vec2(-ax.x,0))*0.175 +
        sampleBloom(pos, vec2(0,r1.y))*0.175 +
        sampleBloom(pos, vec2(0,-r1.y))*0.175;

    // ===== MID =====
    vec3 mid =
        sampleBloom(pos, vec2(r2.x,r2.y))*0.25 +
        sampleBloom(pos, vec2(-r2.x,r2.y))*0.25 +
        sampleBloom(pos, vec2(r2.x,-r2.y))*0.25 +
        sampleBloom(pos, vec2(-r2.x,-r2.y))*0.25;

    // ===== FAR =====
    vec3 far =
        sampleBloom(pos, vec2(r3.x,0))*0.25 +
        sampleBloom(pos, vec2(-r3.x,0))*0.25 +
        sampleBloom(pos, vec2(0,r3.y))*0.25 +
        sampleBloom(pos, vec2(0,-r3.y))*0.25;

    vec3 bloom = near*0.6 + mid*0.3 + far*0.1;

    // ===== HDR ROLLOFF =====
    bloom = bloom / (1.0 + bloom);

    // ===== COLOR SHIFT (film warm highlight) =====
    vec3 warm = vec3(1.05, 1.0, 0.95);
    bloom *= mix(vec3(1.0), warm, 0.2);

    // ===== LENS DIRT (procedural fake) =====
    vec2 uv = pos;
    float dirtMask = fract(sin(dot(floor(uv*vec2(240.0,180.0)), vec2(12.9898,78.233))) * 43758.5453);
    dirtMask = pow(dirtMask, 3.0);

    bloom += bloom * dirtMask * DIRT * 0.5;

    // ===== FINAL BLEND =====
    vec3 result = base + bloom * STRENGTH;

    // tone-map tránh cháy
    result = base + bloom * STRENGTH;

    return vec4(result, 1.0);
}