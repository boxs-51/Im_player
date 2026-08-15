#include "AudioProcessor.h"
#include "log.h"
#include "threads/thread_manager.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <chrono>

AudioProcessor::AudioProcessor()
    : m_threadId("") {}

AudioProcessor::~AudioProcessor() {
    Stop();
}

bool AudioProcessor::Init(SpscRingBuffer<AudioBlock>* inputStream, SpscRingBuffer<AudioBlock>* outputStream) {
    if (!inputStream || !outputStream) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
            std::cout << "[AudioProcessor] Init failed: input or output stream pointer is null.");
        return false;
    }

    m_inputStream = inputStream;
    m_outputStream = outputStream;
    m_threadId = "AudioProcessor_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    return true;
}

void AudioProcessor::Start() {
    if (m_isRunning.load(std::memory_order_relaxed)) return;

    m_isRunning.store(true, std::memory_order_release);
    m_processThread = std::thread(&AudioProcessor::ProcessLoop, this);
    GetThreadManager().Register(m_threadId, &m_processThread);
}

void AudioProcessor::Stop() {
    if (!m_isRunning.exchange(false, std::memory_order_acq_rel)) return;

    if (m_processThread.joinable()) {
        m_processThread.join();
    }

    GetThreadManager().Unregister(m_threadId);

    m_inputStream = nullptr;
    m_outputStream = nullptr;
}

bool AudioProcessor::GetLatestVisualizerData(AudioVisualizerFrame& outFrame) {
    // Đọc Lock-free snapshot mới nhất từ active buffer index
    int activeIdx = m_writeIndex.load(std::memory_order_acquire);
    outFrame = m_visualizerFrames[activeIdx];
    return true;
}

void AudioProcessor::AnalyzeBlock(const AudioBlock& block) {
    // Xác định buffer mục tiêu để ghi (Double buffering)
    int currentIdx = m_writeIndex.load(std::memory_order_relaxed);
    int targetIdx = 1 - currentIdx;
    AudioVisualizerFrame& frame = m_visualizerFrames[targetIdx];

    frame.sequence = block.sequence;
    frame.pts = block.pts;

    const size_t frames = block.frames;
    const uint8_t channels = block.format.channels > 0 ? block.format.channels : 2;

    if (frames == 0 || block.samples.empty()) {
        frame.rmsLeft = 0.0f;
        frame.rmsRight = 0.0f;
        frame.peakLeft = 0.0f;
        frame.peakRight = 0.0f;
        std::fill(std::begin(frame.spectrum), std::end(frame.spectrum), 0.0f);
        m_writeIndex.store(targetIdx, std::memory_order_release);
        return;
    }

    float sumSqL = 0.0f;
    float sumSqR = 0.0f;
    float maxL = 0.0f;
    float maxR = 0.0f;

    // Duyệt mẫu theo kênh thực tế và kiểm tra bounds an toàn
    const size_t maxSampleIndex = block.samples.size();

    if (channels >= 2) {
        for (size_t i = 0; i < frames; ++i) {
            size_t idxL = i * channels;
            size_t idxR = i * channels + 1;

            if (idxR >= maxSampleIndex) break;

            float sampleL = block.samples[idxL];
            float sampleR = block.samples[idxR];

            sumSqL += sampleL * sampleL;
            sumSqR += sampleR * sampleR;

            maxL = std::max(maxL, std::abs(sampleL));
            maxR = std::max(maxR, std::abs(sampleR));
        }
    } else { // Mono fallback
        for (size_t i = 0; i < frames; ++i) {
            if (i >= maxSampleIndex) break;

            float sample = block.samples[i];
            sumSqL += sample * sample;
            maxL = std::max(maxL, std::abs(sample));
        }
        sumSqR = sumSqL;
        maxR = maxL;
    }

    frame.rmsLeft = std::sqrt(sumSqL / static_cast<float>(frames));
    frame.rmsRight = std::sqrt(sumSqR / static_cast<float>(frames));
    frame.peakLeft = maxL;
    frame.peakRight = maxR;

    // Phân tích dải phổ giả lập / Spectrum Bins
    for (size_t b = 0; b < kSpectrumBins; ++b) {
        float factor = static_cast<float>(b + 1) / static_cast<float>(kSpectrumBins);
        frame.spectrum[b] = (frame.rmsLeft + frame.rmsRight) * 0.5f * (1.0f - 0.3f * std::abs(factor - 0.5f));
    }

    // Hoàn tất ghi dữ liệu, cập nhật m_writeIndex bằng memory order release
    m_writeIndex.store(targetIdx, std::memory_order_release);
}

void AudioProcessor::ProcessLoop() {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
        std::cout << "[AudioProcessor] Processing thread started.");

    while (m_isRunning.load(std::memory_order_relaxed)) {
        if (!m_inputStream || !m_outputStream) break;

        // 1. ĐỌC TỪ CAPTURE RING BUFFER (Input Stream)
        const AudioBlock* inSlot = m_inputStream->acquire_read();
        if (!inSlot) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // 2. Phân tích dữ liệu âm thanh cho UI / Visualizer
        AnalyzeBlock(*inSlot);

        // 3. GHI SANG OUTPUT WORKER RING BUFFER (Output Stream)
        // Nếu output buffer đầy, chờ tối đa vài ms trước khi drop để tránh block pipeline quá lâu
        AudioBlock* outSlot = m_outputStream->acquire_write();
        int retryCount = 0;
        
        while (!outSlot && m_isRunning.load(std::memory_order_relaxed) && retryCount < 5) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            outSlot = m_outputStream->acquire_write();
            retryCount++;
        }

        if (outSlot) {
            *outSlot = *inSlot; // Copy dữ liệu block
            m_outputStream->commit_write();
        }

        // 4. Giải phóng Read Slot ở Input Stream
        m_inputStream->release_read();
    }

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
        std::cout << "[AudioProcessor] Processing thread finished.");
}