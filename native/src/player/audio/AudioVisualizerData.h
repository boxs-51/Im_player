#pragma once

#include <cstdint>
#include <array>

constexpr size_t kSpectrumBins = 64;

/**
 * @struct AudioVisualizerFrame
 * @brief Lưu trữ thông tin biên độ và tần số trích xuất từ AudioBlock cho UI/Visualizer.
 */
struct AudioVisualizerFrame {
    // --- 1. BIÊN ĐỘ TỜI GIAN (Time-domain Metrics) ---
    float peakLeft   = 0.0f; // Peak thực tế kênh trái (0.0f -> 1.0f hoặc cao hơn nếu clipping)
    float peakRight  = 0.0f; // Peak thực tế kênh phải
    float rmsLeft    = 0.0f; // Trung bình bình phương (RMS) kênh trái - đại diện cho mức năng lượng
    float rmsRight   = 0.0f; // RMS kênh phải
    
    float crestFactorLeft  = 1.0f; // Tỷ lệ Peak / RMS (Đo độ động / Dynamic Range ngắn hạn)
    float crestFactorRight = 1.0f; 

    // --- 2. ĐỘ TO CẢM NHẬN (Loudness & Dynamic Range) ---
    float shortTermLUFS = -70.0f; // Độ to cảm nhận ngắn hạn (LUFS - theo chuẩn ITU-R BS.1770)
    float dynamicRange  = 0.0f;   // Độ chênh lệch Peak - RMS (dB)

    // --- 3. ĐỌ AN TOÀN TÍN HIỆU (Signal Integrity & Clipping) ---
    bool isClippingLeft  = false; // Cảnh báo quá ngưỡng (0 dB / 1.0f) kênh trái
    bool isClippingRight = false; // Cảnh báo quá ngưỡng kênh phải
    uint32_t clipCount   = 0;     // Số mẫu sample bị tràn ngưỡng trong frame này

    // --- 4. TẦN SỐ & PHỔ NĂNG LƯỢNG (Frequency Domain - FFT) ---
    std::vector<float> spectrum;  // Mảng phổ FFT (ví dụ: 64, 128, hoặc 256 bands)
    
    // Năng lượng theo dải tần chính (Dùng cực tốt cho Auto EQ / Adaptive Filter)
    float subBassEnergy = 0.0f; // Dải siêu trầm: 20Hz - 60Hz
    float bassEnergy    = 0.0f; // Dải trầm: 60Hz - 250Hz
    float midEnergy     = 0.0f; // Dải trung: 250Hz - 4kHz
    float trebleEnergy  = 0.0f; // Dải cao: 4kHz - 20kHz

    // --- 5. TƯƠNG QUAN PHA KÊNH STEREO (Spatial / Phase Metrics) ---
    float phaseCorrelation = 1.0f; // Độ tương quan pha giữa L và R (-1.0f: Triệt tiêu hoàn toàn, 0.0f: Mono hoàn toàn độc lập, +1.0f: Stereo hoàn hảo/Mono)

    // Sync metadata
    double pts = 0.0;
    uint64_t sequence = 0;
};