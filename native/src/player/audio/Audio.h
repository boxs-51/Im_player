#pragma once

#include "AudioCaptureManager.h"
#include "AudioProcessor.h"
#include "AudioOutputWorker.h"
#include "SpscRingBuffer.h"

#include <atomic>

class PlayerStateSystem;

/**
 * @class Audio
 * @brief Manager cấp cao điều phối toàn bộ Audio Pipeline: 
 *        AudioCaptureManager (Producer) -> AudioProcessor (Intermediate) -> AudioOutputWorker (Consumer).
 */
class Audio {
public:
    Audio();
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    /**
     * @brief Khởi tạo và liên kết các thành phần trong Audio Pipeline
     */
    bool Init(mpv_handle* mpv, PlayerStateSystem* stateSystem);
    
    /**
     * @brief Xử lý sự kiện Seek hoặc đổi track từ phía người dùng
     */
    void OnUserSeek();

    /**
     * @brief Dừng và giải phóng toàn bộ tài nguyên pipeline
     */
    void Shutdown();

    /**
     * @brief Cho phép UI lấy snapshot dữ liệu Spectrum/RMS (Lock-Free)
     */
    bool GetVisualizerData(AudioVisualizerFrame& outFrame);

    /**
     * @brief Đổi Audio Backend linh hoạt ngay tại Runtime (SDL2 / WASAPI)
     */
    bool SwitchBackend(AudioBackendType newBackend);

    /**
     * @brief Kiểm tra trạng thái khởi tạo của Audio System
     */
    bool IsInitialized() const { return m_isInitialized.load(std::memory_order_relaxed); }

private:
    /**
     * @brief Xả sạch tất cả các block tồn đọng trong RingBuffer trung gian
     */
    void FlushProcessedBuffer();

    // Buffer trung gian giữa Processor (Producer) và OutputWorker (Consumer)
    // Capacity = 32 blocks (~1.36s đệm tối đa ở 48kHz, 2048 samples/block)
    SpscRingBuffer<AudioBlock> m_processedAudioBuffer{32};

    AudioCaptureManager m_audioCapture;
    AudioProcessor      m_audioProcessor;
    AudioOutputWorker   m_audioOutput;

    std::atomic<bool> m_isInitialized{false};
};