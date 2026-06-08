/*
@NAME: Cinema_FSR_EASU_8tap
@DESC: Lightweight EASU 8-tap upscaler
@HOOK: PREKERNEL
@BIND: HOOKED
@WIDTH: target_width
@HEIGHT: target_height

@PARAM: FSR_STRENGTH
@LABEL: Upscale Sharpness
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.9
*/

#define LUMA vec3(0.299, 0.587, 0.114)

float getLuma(vec3 c) {
    return dot(c, LUMA);
}

vec3 EASU8(vec2 pos, vec2 size)
{
    vec2 p = pos * size - 0.5;
    vec2 ip = floor(p);
    vec2 f = p - ip;

    vec2 px = 1.0 / size;

    // 4 chính
    vec3 a = HOOKED_tex((ip + vec2(0.5,0.5))*px).rgb;
    vec3 b = HOOKED_tex((ip + vec2(1.5,0.5))*px).rgb;
    vec3 c = HOOKED_tex((ip + vec2(0.5,1.5))*px).rgb;
    vec3 d = HOOKED_tex((ip + vec2(1.5,1.5))*px).rgb;

    // 4 chéo
    vec3 tl = HOOKED_tex((ip + vec2(-0.5,-0.5))*px).rgb;
    vec3 tr = HOOKED_tex((ip + vec2( 2.5,-0.5))*px).rgb;
    vec3 bl = HOOKED_tex((ip + vec2(-0.5, 2.5))*px).rgb;
    vec3 br = HOOKED_tex((ip + vec2( 2.5, 2.5))*px).rgb;

    float la=getLuma(a), lb=getLuma(b), lc=getLuma(c), ld=getLuma(d);
    float ltl=getLuma(tl), ltr=getLuma(tr), lbl=getLuma(bl), lbr=getLuma(br);

    // Gradient estimate (improved)
    float gx = abs(la-lb) + abs(lc-ld) + 0.5*(abs(ltl-ltr)+abs(lbl-lbr));
    float gy = abs(la-lc) + abs(lb-ld) + 0.5*(abs(ltl-lbl)+abs(ltr-lbr));

    float s = pow(FSR_STRENGTH,1.3)*0.7;

    // directional blend
    float wx = smoothstep(0.0,1.0+gx, f.x + s*(gx-gy));
    float wy = smoothstep(0.0,1.0+gy, f.y + s*(gy-gx));

    wx = clamp(wx,0.05,0.95);
    wy = clamp(wy,0.05,0.95);

    vec3 hor = mix(a,b,wx);
    vec3 hor2 = mix(c,d,wx);

    return mix(hor,hor2,wy);
}

vec4 hook()
{
    vec3 color = EASU8(HOOKED_pos,HOOKED_res);
    return vec4(color,1.0);
}