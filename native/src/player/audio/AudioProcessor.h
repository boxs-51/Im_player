#pragma once

#include "AudioTypes.h"
#include "SpscRingBuffer.h"
#include "AudioVisualizerData.h"
#include <atomic>
#include <thread>

/**
 * @class AudioProcessor
 * @brief Thread duy nhất nhận Raw Audio từ CaptureManager, phân tích Visualizer 
 *        và chuyển giao Zero-Copy dữ liệu sang OutputWorker.
 */
class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;

    bool Init(SpscRingBuffer<AudioBlock>* inputStream, SpscRingBuffer<AudioBlock>* outputStream);
    void Start();
    void Stop();

    // UI Thread gọi hàm này để lấy snapshot dữ liệu visualizer (HOÀN TOÀN LOCK-FREE)
    bool GetLatestVisualizerData(AudioVisualizerFrame& outFrame);

private:
    void ProcessLoop();
    void AnalyzeBlock(const AudioBlock& block);

    SpscRingBuffer<AudioBlock>* m_inputStream = nullptr;
    SpscRingBuffer<AudioBlock>* m_outputStream = nullptr;

    std::thread m_processThread;
    std::atomic<bool> m_isRunning{false};

    // --- LOCK-FREE VISUALIZER SNAPSHOT (Atomic Exchange Double/Triple Buffering) ---
    AudioVisualizerFrame m_visualizerFrames[2];
    std::atomic<int> m_writeIndex{0};
    std::atomic<uint64_t> m_visualizerSequence{0};
};