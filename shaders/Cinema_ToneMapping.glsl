/*
@NAME: Cinema_ToneMapping_ACES
@DESC: ACES Filmic Tone Mapping (Narkowicz Approximation)
@HOOK: MAIN
@BIND: HOOKED

@PARAM: EXPOSURE
@LABEL: Độ phơi sáng (Exposure)
@MIN: 0.1
@MAX: 3.0
@DEFAULT: 1.0
@INFO: Điều chỉnh cường độ ánh sáng đi vào bộ lọc.

@PARAM: SATURATION
@LABEL: Độ rực màu (Saturation)
@MIN: 0.0
@MAX: 2.0
@DEFAULT: 1.05
@INFO: Bù đắp lượng màu bị mất sau khi nén dải sáng (Desaturation).

@PARAM: GAMMA
@LABEL: Hiệu chỉnh Gamma
@MIN: 0.5
@MAX: 1.5
@DEFAULT: 1.0
@INFO: Điều chỉnh độ sáng vùng trung (Midtones).
*/

#define LUMA vec3(0.2126, 0.7152, 0.0722)

// Thuật toán ACES Filmic Tone Mapping (Xấp xỉ Narkowicz)
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec4 hook() {
    vec3 color = HOOKED_tex(HOOKED_pos).rgb;

    // 1. Áp dụng Exposure (Phơi sáng)
    // Tăng giá trị này nếu video gốc hơi tối hoặc sau khi Bloom làm ảnh bị dịu đi
    color *= EXPOSURE;

    // 2. Thực hiện Tone Mapping
    // Bước này sẽ nén các vùng quá sáng (highlights) không bị cháy trắng 
    // và tạo đường cong tương phản kiểu phim (filmic curve)
    vec3 mapped = ACESFilm(color);

    // 3. Hiệu chỉnh Saturation (Độ rực)
    // Tone mapping thường làm giảm độ bão hòa màu ở vùng sáng, cần bù lại một chút
    float luma = dot(mapped, LUMA);
    mapped = mix(vec3(luma), mapped, SATURATION);

    // 4. Hiệu chỉnh Gamma cuối cùng
    mapped = pow(mapped, vec3(1.0 / max(GAMMA, 0.01)));

    return vec4(mapped, 1.0);
}