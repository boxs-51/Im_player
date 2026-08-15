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
    // Đọc Lock-free snapshot mới nhất
    int activeIdx = m_writeIndex.load(std::memory_order_relaxed);
    outFrame = m_visualizerFrames[activeIdx];
    return true;
}

void AudioProcessor::AnalyzeBlock(const AudioBlock& block) {
    int targetIdx = 1 - m_writeIndex.load(std::memory_order_relaxed);
    AudioVisualizerFrame& frame = m_visualizerFrames[targetIdx];

    frame.sequence = block.sequence;
    frame.pts = block.pts;

    float sumSqL = 0.0f;
    float sumSqR = 0.0f;
    float maxL = 0.0f;
    float maxR = 0.0f;

    const size_t frames = block.frames;
    const uint8_t channels = block.format.channels > 0 ? block.format.channels : 2;

    if (frames == 0) return;

    // Duyệt mẫu an toàn theo kênh thực tế
    if (channels >= 2) {
        for (size_t i = 0; i < frames; ++i) {
            float sampleL = block.samples[i * channels];
            float sampleR = block.samples[i * channels + 1];

            sumSqL += sampleL * sampleL;
            sumSqR += sampleR * sampleR;

            maxL = std::max(maxL, std::abs(sampleL));
            maxR = std::max(maxR, std::abs(sampleR));
        }
    } else { // Mono fallback
        for (size_t i = 0; i < frames; ++i) {
            float sample = block.samples[i];
            sumSqL += sample * sample;
            maxL = std::max(maxL, std::abs(sample));
        }
        sumSqR = sumSqL;
        maxR = maxL;
    }

    frame.rmsLeft = std::sqrt(sumSqL / frames);
    frame.rmsRight = std::sqrt(sumSqR / frames);
    frame.peakLeft = maxL;
    frame.peakRight = maxR;

    // Phân tích dải phổ giả lập / FFT
    for (size_t b = 0; b < kSpectrumBins; ++b) {
        float factor = static_cast<float>(b + 1) / kSpectrumBins;
        frame.spectrum[b] = (frame.rmsLeft + frame.rmsRight) * 0.5f * (1.0f - 0.3f * std::abs(factor - 0.5f));
    }

    // Swapping index không dùng Lock
    m_writeIndex.store(targetIdx, std::memory_order_release);
}

void AudioProcessor::ProcessLoop() {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioProcessor] Processing thread started.");

    while (m_isRunning) {
        // ZERO-COPY READ TỪ CAPTURE RING BUFFER
        const AudioBlock* inSlot = m_inputStream->acquire_read();
        if (!inSlot) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // 1. Phân tích dữ liệu âm thanh cho UI / Visualizer
        AnalyzeBlock(*inSlot);

        // 2. ZERO-COPY WRITE SANG OUTPUT WORKER RING BUFFER
        AudioBlock* outSlot = m_outputStream->acquire_write();
        if (outSlot) {
            *outSlot = *inSlot; // Direct Memory Copy vào Ring Buffer Slot mới
            m_outputStream->commit_write();
        }

        // 3. Giải phóng Read Slot ở Input Stream
        m_inputStream->release_read();
    }

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioProcessor] Processing thread finished.");
}