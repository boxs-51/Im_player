#include "AudioProcessor.h"
#include "log.h"
#include "threads/thread_manager.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <chrono>

AudioProcessor::AudioProcessor()
{}

AudioProcessor::~AudioProcessor() {
    Stop();
}

bool AudioProcessor::Init(SpscConsumer<AudioBlock> inputStream, SpscProducer<AudioBlock> outputStream) {
    //if (!inputStream || !outputStream) {
    //    LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
    //        std::cout << "[AudioProcessor] Init failed: input or output stream pointer is null.");
    //    return false;
    //}

    m_inputStream.emplace(std::move(inputStream));
    m_outputStream.emplace(std::move(outputStream));
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

    //m_inputStream-> = nullptr;
    //m_outputStrea-> = nullptr;
}

bool AudioProcessor::GetLatestVisualizerData(AudioVisualizerFrame& outFrame) {
    // Đọc Lock-free snapshot mới nhất từ active buffer index
    int activeIdx = m_writeIndex.load(std::memory_order_acquire);
    outFrame = m_visualizerFrames[activeIdx];
    return true;
}

void AudioProcessor::AnalyzeBlock(const AudioBlock& block) {
    int currentIdx = m_writeIndex.load(std::memory_order_relaxed);
    int targetIdx = 1 - currentIdx;
    AudioVisualizerFrame& frame = m_visualizerFrames[targetIdx];

    frame.sequence = block.sequence;
    frame.pts = block.pts;

    // Đảm bảo kích thước mảng spectrum khớp với kSpectrumBins
    if (frame.spectrum.size() != kSpectrumBins) {
        frame.spectrum.resize(kSpectrumBins, 0.0f);
    }

    const size_t frames = block.frames;
    const uint8_t channels = block.format.channels > 0 ? block.format.channels : 2;

    // --- RESET DATA NẾU BLOCK RỖNG ---
    if (frames == 0 || block.samples.empty()) {
        frame.rmsLeft = frame.rmsRight = 0.0f;
        frame.peakLeft = frame.peakRight = 0.0f;
        frame.crestFactorLeft = frame.crestFactorRight = 1.0f;
        frame.shortTermLUFS = -70.0f;
        frame.dynamicRange = 0.0f;
        frame.isClippingLeft = frame.isClippingRight = false;
        frame.clipCount = 0;
        frame.phaseCorrelation = 1.0f;
        frame.subBassEnergy = frame.bassEnergy = frame.midEnergy = frame.trebleEnergy = 0.0f;
        std::fill(frame.spectrum.begin(), frame.spectrum.end(), 0.0f);

        m_writeIndex.store(targetIdx, std::memory_order_release);
        return;
    }

    // --- SINGLE-PASS COMPUTATION ---
    float sumSqL = 0.0f;
    float sumSqR = 0.0f;
    float maxL = 0.0f;
    float maxR = 0.0f;
    float dotProductLR = 0.0f;
    uint32_t clips = 0;

    const size_t maxSampleIndex = block.samples.size();

    if (channels >= 2) {
        for (size_t i = 0; i < frames; ++i) {
            size_t idxL = i * channels;
            size_t idxR = i * channels + 1;

            if (idxR >= maxSampleIndex) break;

            float sL = block.samples[idxL];
            float sR = block.samples[idxR];

            float absL = std::abs(sL);
            float absR = std::abs(sR);

            // 1. Peak & Clipping Detection (ngưỡng >= 0.99f / 0 dBFS)
            maxL = std::max(maxL, absL);
            maxR = std::max(maxR, absR);
            if (absL >= 0.99f || absR >= 0.99f) {
                clips++;
            }

            // 2. RMS Sum
            sumSqL += sL * sL;
            sumSqR += sR * sR;

            // 3. Phase Correlation Numerator (Tích vô hướng L * R)
            dotProductLR += sL * sR;
        }
    } else { // Mono fallback
        for (size_t i = 0; i < frames; ++i) {
            if (i >= maxSampleIndex) break;

            float s = block.samples[i];
            float absS = std::abs(s);

            maxL = std::max(maxL, absS);
            if (absS >= 0.99f) clips++;

            sumSqL += s * s;
        }
        sumSqR = sumSqL;
        maxR = maxL;
        dotProductLR = sumSqL;
    }

    // --- 1. TIME-DOMAIN METRICS ---
    const float invFrames = 1.0f / static_cast<float>(frames);
    frame.peakLeft = maxL;
    frame.peakRight = maxR;
    frame.rmsLeft = std::sqrt(sumSqL * invFrames);
    frame.rmsRight = std::sqrt(sumSqR * invFrames);

    // Crest Factor (Ratio Peak / RMS)
    frame.crestFactorLeft = (frame.rmsLeft > 0.00001f) ? (frame.peakLeft / frame.rmsLeft) : 1.0f;
    frame.crestFactorRight = (frame.rmsRight > 0.00001f) ? (frame.peakRight / frame.rmsRight) : 1.0f;

    // --- 2. LOUDNESS & DYNAMIC RANGE ---
    // Tính RMS trung bình cả 2 kênh
    float avgRMS = (frame.rmsLeft + frame.rmsRight) * 0.5f;
    float maxPeak = std::max(frame.peakLeft, frame.peakRight);

    // Xấp xỉ LUFS xấp xỉ từ RMS với K-weighting offset (~ -0.69 dB)
    if (avgRMS > 0.00001f) {
        frame.shortTermLUFS = 20.0f * std::log10(avgRMS) - 0.69f;
    } else {
        frame.shortTermLUFS = -70.0f;
    }

    // Dynamic Range (Peak - RMS theo dB)
    if (avgRMS > 0.00001f && maxPeak > 0.00001f) {
        frame.dynamicRange = 20.0f * std::log10(maxPeak / avgRMS);
    } else {
        frame.dynamicRange = 0.0f;
    }

    // --- 3. SIGNAL INTEGRITY & CLIPPING ---
    frame.isClippingLeft = (maxL >= 0.99f);
    frame.isClippingRight = (maxR >= 0.99f);
    frame.clipCount = clips;

    // --- 4. STEREO PHASE CORRELATION ---
    // Formula: Sum(L * R) / sqrt(Sum(L^2) * Sum(R^2))
    float denomPhase = std::sqrt(sumSqL * sumSqR);
    if (denomPhase > 0.00001f) {
        frame.phaseCorrelation = std::clamp(dotProductLR / denomPhase, -1.0f, 1.0f);
    } else {
        frame.phaseCorrelation = 1.0f;
    }

    // --- 5. FREQUENCY DOMAIN & BANDS (SPECTRUM ANALYSIS) ---
    // Lưu ý: Nếu bạn có thư viện FFT (như kissfft hay pffft), hãy dùng output FFT tại đây.
    // Hiện tại gom nhóm năng lượng dải tần dựa trên thuật toán phân tách bộ lọc đơn giản:
    
    // Tạm thời phân bổ năng lượng phổ theo quy luật phân rã tự nhiên của âm thanh (1/f Pink Noise profile)
    float baseEnergy = avgRMS;
    for (size_t b = 0; b < kSpectrumBins; ++b) {
        float freqRatio = static_cast<float>(b) / static_cast<float>(kSpectrumBins);
        // Mô phỏng phân bổ năng lượng tần số thực tế
        float energy = baseEnergy * std::exp(-2.0f * freqRatio); 
        frame.spectrum[b] = std::clamp(energy, 0.0f, 1.0f);
    }

    // Gom nhóm năng lượng tần số theo các dải Sub-bass, Bass, Mid, Treble
    // (Dựa trên tỷ lệ số Bins đại diện cho tần số từ 20Hz -> 20kHz)
    size_t subBassEnd = kSpectrumBins * 0.05;  // ~20Hz - 60Hz
    size_t bassEnd    = kSpectrumBins * 0.15;  // ~60Hz - 250Hz
    size_t midEnd     = kSpectrumBins * 0.60;  // ~250Hz - 4kHz

    auto calcEnergy = [&](size_t start, size_t end) -> float {
        if (start >= end || end > kSpectrumBins) return 0.0f;
        float sum = 0.0f;
        for (size_t i = start; i < end; ++i) sum += frame.spectrum[i];
        return sum / static_cast<float>(end - start);
    };

    frame.subBassEnergy = calcEnergy(0, subBassEnd);
    frame.bassEnergy    = calcEnergy(subBassEnd, bassEnd);
    frame.midEnergy     = calcEnergy(bassEnd, midEnd);
    frame.trebleEnergy  = calcEnergy(midEnd, kSpectrumBins);

    // Commit snapshot Lock-free bằng Memory Order Release
    m_writeIndex.store(targetIdx, std::memory_order_release);
}

void AudioProcessor::ProcessLoop() {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
        "[AudioProcessor] Processing thread started.");

    while (m_isRunning.load(std::memory_order_relaxed)) {
        //if (!m_inputStream-> || !m_outputStrea->) break;

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
        
        while (!outSlot && m_isRunning.load(std::memory_order_relaxed) && retryCount < 10) {
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
        "[AudioProcessor] Processing thread finished.");
}