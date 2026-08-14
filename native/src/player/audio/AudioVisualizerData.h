#pragma once

#include <cstdint>
#include <array>

constexpr size_t kSpectrumBins = 64;

/**
 * @struct AudioVisualizerFrame
 * @brief Lưu trữ thông tin biên độ và tần số trích xuất từ AudioBlock cho UI/Visualizer.
 */
struct AudioVisualizerFrame {
    uint64_t sequence = 0;
    double pts = 0.0;

    float rmsLeft  = 0.0f; // Root Mean Square (âm lượng trung bình kênh trái)
    float rmsRight = 0.0f; // Root Mean Square kênh phải
    float peakLeft = 0.0f; // Đỉnh sóng kênh trái
    float peakRight= 0.0f; // Đỉnh sóng kênh phải

    // Mảng mô phỏng phổ tần số (Spectrum) dành cho thanh cột visualizer
    std::array<float, kSpectrumBins> spectrum{};
};