/*
@NAME: Bloom Cinema Pro
@DESC: Bloom điện ảnh tối ưu (Gaussian + Threshold)
@HOOK: LINEAR

@PARAM: THRESHOLD
@LABEL: Ngưỡng lọc sáng
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.6

@PARAM: STRENGTH
@LABEL: Cường độ bloom
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 0.5

@PARAM: RANGE
@LABEL: Bán kính blur
@MIN: 1
@MAX: 5
@DEFAULT: 2
*/

vec4 hook() {
    vec4 base = HOOKED_tex(HOOKED_pos);

    int r = int(RANGE);

    vec3 bloom = vec3(0.0);
    float weightSum = 0.0;

    for (int x = -r; x <= r; x++) {
        for (int y = -r; y <= r; y++) {

            vec2 offset = vec2(x, y) * HOOKED_pt;
            vec3 sample = HOOKED_tex(HOOKED_pos + offset).rgb;

            float brightness = dot(sample, vec3(0.2126, 0.7152, 0.0722));
            if (brightness < THRESHOLD) continue;

            float dist = float(x*x + y*y);
            float w = exp(-dist / (2.0 * float(r*r)));

            bloom += sample * w;
            weightSum += w;
        }
    }

    if (weightSum > 0.0)
        bloom /= weightSum;

    return vec4(base.rgb + bloom * STRENGTH, base.a);
}