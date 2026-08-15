#pragma once

#include "AudioTypes.h"
#include "SpscRingBuffer.h"
#include "AudioVisualizerData.h"
#include "threads/thread_id.h"

#include <atomic>
#include <thread>

/**
 * @class AudioProcessor
 * @brief Thread duy nhất nhận Raw Audio từ CaptureManager, phân tích dữ liệu 
 *        Visualizer (RMS, Peak, Spectrum) và chuyển giao dữ liệu sang OutputWorker.
 */
class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;

    /**
     * @brief Khởi tạo Processor với các RingBuffer đầu vào và đầu ra
     */
    bool Init(SpscRingBuffer<AudioBlock>* inputStream, SpscRingBuffer<AudioBlock>* outputStream);
    
    void Start();
    void Stop();

    /**
     * @brief UI Thread gọi hàm này để lấy snapshot dữ liệu visualizer (HOÀN TOÀN LOCK-FREE)
     */
    bool GetLatestVisualizerData(AudioVisualizerFrame& outFrame);

    /**
     * @brief Kiểm tra xem Processor có đang chạy hay không
     */
    bool IsRunning() const { return m_isRunning.load(std::memory_order_relaxed); }

private:
    void ProcessLoop();
    void AnalyzeBlock(const AudioBlock& block);

    // --- Stream Interfaces ---
    SpscRingBuffer<AudioBlock>* m_inputStream = nullptr;
    SpscRingBuffer<AudioBlock>* m_outputStream = nullptr;

    // --- Thread Control ---
    std::thread m_processThread;
    std::atomic<bool> m_isRunning{false};
    ThreadID m_threadId;

    // --- LOCK-FREE VISUALIZER SNAPSHOT (Double Buffering) ---
    AudioVisualizerFrame m_visualizerFrames[2];
    std::atomic<int> m_writeIndex{0};
    std::atomic<uint64_t> m_visualizerSequence{0};
};