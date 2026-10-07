#pragma once

#include "AudioTypes.h"
#include "SpscRingBuffer.h"
#include "AudioVisualizerData.h"
#include "AudioVisualizerSnapshot.h"
#include "threads/thread_id.h"

#include <optional>
#include <atomic>
#include <thread>
#include <deque>

#include "pffft/pffft.h"

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
    bool Init(SpscConsumer<AudioBlock> inputStream, SpscProducer<AudioBlock> outputStream);
    
    void Start();
    void Stop();

    /**
     * @brief UI Thread lấy bản sao visualizer qua snapshot boundary đồng bộ, race-free.
     */
    bool GetLatestVisualizerData(AudioVisualizerFrame& outFrame);

    /**
     * @brief Kiểm tra xem Processor có đang chạy hay không
     */
    bool IsRunning() const { return m_isRunning.load(std::memory_order_relaxed); }

private:
    void ProcessLoop();
    void AnalysisLoop();
    void AnalyzeBlock(const AudioBlock& block);

    // --- Stream Interfaces ---
    std::optional<SpscConsumer<AudioBlock>> m_inputStream;
    std::optional<SpscProducer<AudioBlock>> m_outputStream;

    // Issue #31: audible PCM forwarding must not wait for heavy visualizer/loudness
    // analysis. The processor thread is the sole producer; the analysis thread is
    // the sole consumer. Full queue => drop analysis work, never PCM forwarding.
    static constexpr size_t kAnalysisQueueCapacity = 32;
    std::optional<SpscProducer<AudioBlock>> m_analysisProducer;
    std::optional<SpscConsumer<AudioBlock>> m_analysisConsumer;

    // --- Thread Control ---
    std::thread m_processThread;
    std::thread m_analysisThread;
    std::atomic<bool> m_isRunning{false};
    ThreadID m_threadId = "";
    ThreadID m_analysisThreadId = "";

    // Context & Buffers của PFFFT
    PFFFT_Setup* m_pffftSetup = nullptr;
    float* m_fftInput = nullptr;
    float* m_fftOutput = nullptr;
    float* m_fftWork = nullptr;

    // Ring buffer tích lũy samples PCM mono cho FFT
    std::vector<float> m_fftBuffer;
    size_t m_fftBufferPos = 0;

    // Sliding Window Buffers cho Loudness Metrics (giả định 48kHz, ~100 blocks/sec)
    std::deque<float> m_momentaryWindow; // ~400ms (chứa khoảng 40 blocks)
    std::deque<float> m_shortTermWindow; // ~3.0s (chứa khoảng 300 blocks)
    std::vector<float> m_lraBlocks;      // Lưu trữ các block Short-Term để tính LRA

    // Tích lũy cho Integrated LUFS từ đầu stream
    float m_integratedSumSq = 0.0f;
    uint64_t m_integratedSampleCount = 0;

    // Reset lịch sử đo đạc khi chuyển bài hoặc Seek
    void ResetLoudnessHistory();

    // --- RACE-FREE VISUALIZER SNAPSHOT ---
    // Processor owns the working frame; only publication/copy takes the mutex.
    AudioVisualizerFrame m_workingVisualizerFrame;
    AudioVisualizerSnapshot m_visualizerSnapshot;
};