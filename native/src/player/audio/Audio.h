#pragma once

#include "AudioCaptureManager.h"
#include "AudioProcessor.h"
#include "AudioOutputWorker.h"
#include "SpscRingBuffer.h"

class PlayerStateSystem;

/**
 * @class Audio
 * @brief Điều phối toàn bộ Audio Pipeline: Capture -> Processor -> Output Worker
 */
class Audio {
public:
    Audio();
    ~Audio();

    // Khởi tạo toàn bộ Audio Pipeline
    bool Init(mpv_handle* mpv, PlayerStateSystem* stateSystem);
    
    // Gọi khi người dùng Seek hoặc đổi Track/Media
    void OnUserSeek();

    // Dừng và dọn dẹp Pipeline
    void Shutdown();

    // Cho phép UI lấy snapshot dữ liệu Spectrum/RMS (Lock-Free)
    bool GetVisualizerData(AudioVisualizerFrame& outFrame);

private:
    // Buffer trung gian giữa Processor (Producer) và OutputWorker (Consumer)
    // Capacity = 32 blocks (~1.36s đệm tối đa)
    SpscRingBuffer<AudioBlock> m_processedAudioBuffer{32};

    AudioCaptureManager m_audioCapture;
    AudioProcessor      m_audioProcessor;
    AudioOutputWorker   m_audioOutput;
};