#pragma once

#include "AudioTypes.h"
#include "SpscRingBuffer.h"
#include "AudioVisualizerData.h"
#include <atomic>
#include <thread>
#include <mutex>

class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;

    bool Init(SpscRingBuffer<AudioBlock>* inputStream, SpscRingBuffer<AudioBlock>* outputStream);
    void Start();
    void Stop();

    // UI Thread gọi hàm này để lấy snapshot dữ liệu visualizer mới nhất (thread-safe)
    bool GetLatestVisualizerData(AudioVisualizerFrame& outFrame);

private:
    void ProcessLoop();
    void AnalyzeBlock(const AudioBlock& block);

    SpscRingBuffer<AudioBlock>* m_inputStream = nullptr;
    SpscRingBuffer<AudioBlock>* m_outputStream = nullptr;

    std::thread m_processThread;
    std::atomic<bool> m_isRunning{false};

    // Dữ liệu visualizer chia sẻ với UI thread (bảo vệ bằng mutex nhẹ hoặc lock-free snapshot)
    AudioVisualizerFrame m_latestVisualizerFrame;
    std::mutex m_visualizerMutex;
};