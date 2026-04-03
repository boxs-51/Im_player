/*
@NAME: Bloom Cinema
@DESC: Hiệu ứng ánh sáng rực rỡ Cinema cao cấp
@HOOK: MAIN

@PARAM: THRESHOLD
@LABEL: Ngưỡng lọc sáng (0.0 - 1.0)
@MIN: 0.0
@MAX: 1.0
@DEFAULT: 0.6

@PARAM: STRENGTH
@LABEL: Cường độ phát sáng
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 0.5

@PARAM: RANGE
@LABEL: Độ rộng vùng mờ (Blur)
@MIN: 1
@MAX: 5
@DEFAULT: 2
*/

// --- CODE THỰC THI ---
// Lưu ý: Manager sẽ tự động chèn các dòng #define THRESHOLD, STRENGTH, RANGE vào đây

vec4 hook() {
    vec4 color = HOOKED_tex(HOOKED_pos);
    vec3 bloom = vec3(0.0);
    float count = 0.0;

    // Sử dụng vòng lặp dựa trên tham số RANGE động
    // Ép kiểu sang int vì tham số truyền từ Manager thường là float
    int r = int(RANGE);

    for (int x = -r; x <= r; x++) {
        for (int y = -r; y <= r; y++) {
            vec2 offset = vec2(float(x), float(y)) * HOOKED_pt;
            vec3 sample = HOOKED_tex(HOOKED_pos + offset).rgb;
            
            // Tính độ sáng (Luminance) chuẩn Rec. 709
            float brightness = dot(sample, vec3(0.2126, 0.7152, 0.0722));
            
            if (brightness > THRESHOLD) {
                bloom += sample;
            }
            count += 1.0;
        }
    }
    
    bloom /= count;
    return vec4(color.rgb + (bloom * STRENGTH), color.a);
}