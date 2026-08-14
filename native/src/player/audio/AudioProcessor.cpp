#include "AudioProcessor.h"
#include "log.h"
#include <cmath>
#include <algorithm>
#include <iostream>

AudioProcessor::AudioProcessor() = default;

AudioProcessor::~AudioProcessor() {
    Stop();
}

bool AudioProcessor::Init(SpscRingBuffer<AudioBlock>* inputStream, SpscRingBuffer<AudioBlock>* outputStream) {
    if (!inputStream || !outputStream) return false;
    m_inputStream = inputStream;
    m_outputStream = outputStream;
    return true;
}

void AudioProcessor::Start() {
    if (m_isRunning) return;
    m_isRunning = true;
    m_processThread = std::thread(&AudioProcessor::ProcessLoop, this);
}

void AudioProcessor::Stop() {
    if (!m_isRunning.exchange(false)) return;
    if (m_processThread.joinable()) {
        m_processThread.join();
    }
}

bool AudioProcessor::GetLatestVisualizerData(AudioVisualizerFrame& outFrame) {
    std::lock_guard<std::mutex> lock(m_visualizerMutex);
    outFrame = m_latestVisualizerFrame;
    return true;
}

void AudioProcessor::AnalyzeBlock(const AudioBlock& block) {
    AudioVisualizerFrame frame;
    frame.sequence = block.sequence;
    frame.pts = block.pts;

    float sumSqL = 0.0f;
    float sumSqR = 0.0f;
    float maxL = 0.0f;
    float maxR = 0.0f;

    size_t frames = block.frames;
    if (frames == 0) return;

    // Duyệt qua các mẫu âm thanh để tính toán RMS và Peak
    for (size_t i = 0; i < frames; ++i) {
        float sampleL = block.samples[i * 2];
        float sampleR = block.samples[i * 2 + 1];

        sumSqL += sampleL * sampleL;
        sumSqR += sampleR * sampleR;

        maxL = std::max(maxL, std::abs(sampleL));
        maxR = std::max(maxR, std::abs(sampleR));
    }

    frame.rmsLeft = std::sqrt(sumSqL / frames);
    frame.rmsRight = std::sqrt(sumSqR / frames);
    frame.peakLeft = maxL;
    frame.peakRight = maxR;

    // Điền dữ liệu giả lập/phân đoạn phổ tần số cơ bản (có thể thay thế bằng FFT chuyên sâu sau)
    for (size_t b = 0; b < kSpectrumBins; ++b) {
        float factor = static_cast<float>(b + 1) / kSpectrumBins;
        frame.spectrum[b] = (frame.rmsLeft + frame.rmsRight) * 0.5f * (1.0f - 0.3f * std::abs(factor - 0.5f));
    }

    // Cập nhật snapshot cho UI
    {
        std::lock_guard<std::mutex> lock(m_visualizerMutex);
        m_latestVisualizerFrame = frame;
    }
}

void AudioProcessor::ProcessLoop() {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioProcessor] Processing thread started.");

    AudioBlock block;
    while (m_isRunning) {
        // Lấy block từ input stream (từ CaptureManager)
        if (m_inputStream->try_pop(block)) {
            
            // 1. Phân tích dữ liệu âm thanh cho Visualizer / UI
            AnalyzeBlock(block);

            // 2. [Mở rộng tương lai]: Chèn các bộ lọc Equalizer / Effects tại đây nếu cần

            // 3. Đẩy block sang output stream để AudioOutputWorker tiêu thụ phát ra loa
            while (m_isRunning && !m_outputStream->try_push(std::move(block))) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioProcessor] Processing thread finished.");
}