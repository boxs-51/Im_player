#include "AudioProcessor.h"
#include "log.h"
#include "threads/thread_manager.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <chrono>


AudioProcessor::AudioProcessor() {
    // 1. Khởi tạo PFFFT Setup cho số thực (PFFFT_REAL)
    m_pffftSetup = pffft_new_setup(kFftSize, PFFFT_REAL);

    // 2. Cấp phát bộ nhớ căn chỉnh (SIMD aligned memory) bằng hàm của PFFFT
    m_fftInput  = (float*)pffft_aligned_malloc(kFftSize * sizeof(float));
    m_fftOutput = (float*)pffft_aligned_malloc(kFftSize * sizeof(float));
    m_fftWork   = (float*)pffft_aligned_malloc(kFftSize * sizeof(float));

    m_fftBuffer.resize(kFftSize, 0.0f);
}

AudioProcessor::~AudioProcessor() {
    Stop();

    // Giải phóng tài nguyên PFFFT
    if (m_pffftSetup) {
        pffft_destroy_setup(m_pffftSetup);
        m_pffftSetup = nullptr;
    }
    if (m_fftInput)  pffft_aligned_free(m_fftInput);
    if (m_fftOutput) pffft_aligned_free(m_fftOutput);
    if (m_fftWork)   pffft_aligned_free(m_fftWork);
}

bool AudioProcessor::Init(SpscConsumer<AudioBlock> inputStream, SpscProducer<AudioBlock> outputStream) {
    //if (!inputStream || !outputStream) {
    //    LOG(1, LogLevel::Error, LogCategory::Audio, 
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

    if (frame.spectrum.size() != kSpectrumBins) {
        frame.spectrum.resize(kSpectrumBins, 0.0f);
    }

    const size_t frames = block.frames;
    const uint8_t channels = block.format.channels > 0 ? block.format.channels : 2;

    if (frames == 0 || block.samples.empty()) {
        // Reset frame data nếu block rỗng
        frame.rmsLeft = frame.rmsRight = 0.0f;
        frame.peakLeft = frame.peakRight = 0.0f;
        frame.crestFactorLeft = frame.crestFactorRight = 1.0f;
        frame.momentaryLUFS = frame.shortTermLUFS = frame.integratedLUFS = -70.0f;
        frame.loudnessRange = frame.dynamicRange = 0.0f;
        frame.isClippingLeft = frame.isClippingRight = false;
        frame.clipCount = 0;
        frame.truePeakEst = -99.0f;
        frame.phaseCorrelation = 1.0f;
        frame.subBassEnergy = frame.bassEnergy = frame.midEnergy = frame.trebleEnergy = 0.0f;
        std::fill(frame.spectrum.begin(), frame.spectrum.end(), 0.0f);

        m_writeIndex.store(targetIdx, std::memory_order_release);
        return;
    }

    // --- SINGLE-PASS COMPUTATION ---
    float sumSqL = 0.0f, sumSqR = 0.0f;
    float maxL = 0.0f, maxR = 0.0f;
    float dotProductLR = 0.0f;
    uint32_t clips = 0;
    uint32_t validSamplesCount = 0;
    const size_t maxSampleIndex = block.samples.size();

    auto SanitizeSample = [](float v) -> float {
        if (std::isnan(v) || std::isinf(v)) return 0.0f;
        return std::clamp(v, -10.0f, 10.0f);
    };

    if (channels >= 2) {
        for (size_t i = 0; i < frames; ++i) {
            size_t idxL = i * channels;
            size_t idxR = i * channels + 1;
            if (idxR >= maxSampleIndex) break;

            float sL = SanitizeSample(block.samples[idxL]);
            float sR = SanitizeSample(block.samples[idxR]);
            float absL = std::abs(sL);
            float absR = std::abs(sR);

            maxL = std::max(maxL, absL);
            maxR = std::max(maxR, absR);
            if (absL >= 0.99f || absR >= 0.99f) clips++;

            sumSqL += sL * sL;
            sumSqR += sR * sR;
            dotProductLR += sL * sR;
        }
    } else {
        for (size_t i = 0; i < frames; ++i) {
            if (i >= maxSampleIndex) break;
            float s = SanitizeSample(block.samples[i]);
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

    frame.crestFactorLeft = (frame.rmsLeft > 1e-5f) ? (frame.peakLeft / frame.rmsLeft) : 1.0f;
    frame.crestFactorRight = (frame.rmsRight > 1e-5f) ? (frame.peakRight / frame.rmsRight) : 1.0f;

    // --- 2. CALCULATE FULL ITU-R BS.1770 LOUDNESS METRICS ---
    float blockMeanSq = (sumSqL + sumSqR) * 0.5f * invFrames; // Mean Square của Block hiện tại

    // A. Momentary LUFS (Cửa sổ trượt 400ms - xấp xỉ 20 blocks ở 48kHz block size 1024)
    m_momentaryWindow.push_back(blockMeanSq);
    if (m_momentaryWindow.size() > 20) m_momentaryWindow.pop_front();

    float mSum = 0.0f;
    for (float v : m_momentaryWindow) mSum += v;
    float mMeanSq = mSum / m_momentaryWindow.size();
    frame.momentaryLUFS = (mMeanSq > 1e-7f) ? (10.0f * std::log10(mMeanSq) - 0.69f) : -70.0f;

    // B. Short-Term LUFS (Cửa sổ trượt 3.0s - xấp xỉ 140 blocks)
    m_shortTermWindow.push_back(blockMeanSq);
    if (m_shortTermWindow.size() > 140) m_shortTermWindow.pop_front();

    float sSum = 0.0f;
    for (float v : m_shortTermWindow) sSum += v;
    float sMeanSq = sSum / m_shortTermWindow.size();
    frame.shortTermLUFS = (sMeanSq > 1e-7f) ? (10.0f * std::log10(sMeanSq) - 0.69f) : -70.0f;

    // C. Integrated LUFS (Tích lũy từ đầu bài)
    m_integratedSumSq += (sumSqL + sumSqR) * 0.5f;
    m_integratedSampleCount += frames;
    if (m_integratedSampleCount > 0) {
        float avgMeanSq = static_cast<float>(m_integratedSumSq / m_integratedSampleCount);
        frame.integratedLUFS = (avgMeanSq > 1e-7f) ? (10.0f * std::log10(avgMeanSq) - 0.69f) : -70.0f;
    }

    // E. Loudness Range (LRA) Calculation - ITU-R BS.1770-4 Standard
    // Lưu trữ Short-Term LUFS vào bộ đệm LRA (giữ tối đa ~1000 blocks (~20 giây) để tối ưu RAM & CPU)
    if (frame.shortTermLUFS > -70.0f) { // Absolute Gate Threshold (-70 LUFS)
        m_lraBlocks.push_back(frame.shortTermLUFS);
        if (m_lraBlocks.size() > 1000) {
            m_lraBlocks.erase(m_lraBlocks.begin());
        }
    }

    if (m_lraBlocks.size() >= 20) { // Cần ít nhất ~2-3 giây dữ liệu để bắt đầu tính LRA
        // Bước 1: Tính trung bình năng lượng (Linear Power) của tất cả các block vượt qua Absolute Gate
        double absGatedSumPower = 0.0;
        for (float st_lufs : m_lraBlocks) {
            absGatedSumPower += std::pow(10.0, static_cast<double>(st_lufs + 0.69f) / 10.0);
        }
        double absGatedMeanPower = absGatedSumPower / m_lraBlocks.size();
        float absGatedLUFS = 10.0f * std::log10(absGatedMeanPower) - 0.69f;

        // Bước 2: Relative Gate (Lọc bỏ các block thấp hơn Absolute LUFS - 20 LU)
        float relativeThreshold = absGatedLUFS - 20.0f;
        std::vector<float> relativeGatedBlocks;
        relativeGatedBlocks.reserve(m_lraBlocks.size());

        for (float st_lufs : m_lraBlocks) {
            if (st_lufs >= relativeThreshold) {
                relativeGatedBlocks.push_back(st_lufs);
            }
        }

        // Bước 3: Tính Percentile 10th và 95th trên tập dữ liệu đã lọc qua Relative Gate
        if (relativeGatedBlocks.size() >= 10) {
            std::sort(relativeGatedBlocks.begin(), relativeGatedBlocks.end());
            size_t idx10 = static_cast<size_t>(relativeGatedBlocks.size() * 0.10);
            size_t idx95 = static_cast<size_t>(relativeGatedBlocks.size() * 0.95);

            frame.loudnessRange = relativeGatedBlocks[idx95] - relativeGatedBlocks[idx10];
            
            // Cập nhật LRA Low & High tương ứng cho Context
            frame.lralow = relativeGatedBlocks[idx10];
            frame.lrahigh = relativeGatedBlocks[idx95];
        } else {
            frame.loudnessRange = 0.0f;
        }
    } else {
        frame.loudnessRange = 0.0f;
    }

    float maxPeak = std::max(maxL, maxR);
    float avgRMS = (frame.rmsLeft + frame.rmsRight) * 0.5f;
    frame.dynamicRange = (avgRMS > 1e-5f && maxPeak > 1e-5f) ? (20.0f * std::log10(maxPeak / avgRMS)) : 0.0f;

    // --- 3. SIGNAL INTEGRITY & ESTIMATED TRUE PEAK ---
    frame.isClippingLeft = (maxL >= 0.99f);
    frame.isClippingRight = (maxR >= 0.99f);
    frame.clipCount = clips;
    // Ước tính True Peak bằng Crest Factor heuristic (+0.5dB headroom factor)
    frame.truePeakEst = (maxPeak > 1e-5f) ? (20.0f * std::log10(maxPeak) + 0.5f) : -99.0f;

    // --- 4. STEREO PHASE CORRELATION ---
    float denomPhase = std::sqrt(sumSqL * sumSqR);
    frame.phaseCorrelation = (denomPhase > 1e-5f) ? std::clamp(dotProductLR / denomPhase, -1.0f, 1.0f) : 1.0f;

    // --- 5. FREQUENCY DOMAIN (FFT & BINNING) ---
    if (m_pffftSetup) {
        
        for (size_t i = 0; i < frames; ++i) {
            float monoSample = 0.0f;
            if (channels >= 2) {
                size_t idxL = i * channels;
                size_t idxR = i * channels + 1;
                if (idxR < maxSampleIndex) {
                    // Trộn Stereo thành Mono cho FFT
                    monoSample = (SanitizeSample(block.samples[idxL]) + SanitizeSample(block.samples[idxR])) * 0.5f;
                }
            } else if (i < maxSampleIndex) {
                monoSample = SanitizeSample(block.samples[i]);
            }

            // Đẩy vào Ring Buffer m_fftBuffer
            m_fftBuffer[m_fftBufferPos] = monoSample;
            m_fftBufferPos = (m_fftBufferPos + 1) % kFftSize;
        }

        for (size_t i = 0; i < kFftSize; ++i) {
            size_t readIdx = (m_fftBufferPos + i) % kFftSize;
            float hannMultiplier = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (kFftSize - 1)));
            m_fftInput[i] = m_fftBuffer[readIdx] * hannMultiplier;
        }

        pffft_transform_ordered(m_pffftSetup, m_fftInput, m_fftOutput, m_fftWork, PFFFT_FORWARD);

        size_t numBinsNyquist = kFftSize / 2;
        for (size_t b = 0; b < kSpectrumBins; ++b) {
            float logStart = std::pow(static_cast<float>(numBinsNyquist), static_cast<float>(b) / kSpectrumBins) - 1.0f;
            float logEnd   = std::pow(static_cast<float>(numBinsNyquist), static_cast<float>(b + 1) / kSpectrumBins) - 1.0f;

            size_t startBin = std::clamp(static_cast<size_t>(logStart), (size_t)1, numBinsNyquist - 1);
            size_t endBin   = std::clamp(static_cast<size_t>(logEnd), startBin + 1, numBinsNyquist);

            float binEnergy = 0.0f;
            for (size_t k = startBin; k < endBin; ++k) {
                float re = m_fftOutput[2 * k];
                float im = m_fftOutput[2 * k + 1];
                binEnergy += std::sqrt(re * re + im * im) / (kFftSize / 2.0f);
            }

            binEnergy /= static_cast<float>(endBin - startBin);
            float db = 20.0f * std::log10(std::max(binEnergy, 0.00001f));
            frame.spectrum[b] = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
        }
    }

    size_t subBassEnd = kSpectrumBins * 0.05;
    size_t bassEnd    = kSpectrumBins * 0.15;
    size_t midEnd     = kSpectrumBins * 0.60;

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

    // Commit Lock-Free
    m_writeIndex.store(targetIdx, std::memory_order_release);
}

void AudioProcessor::ProcessLoop() {
    LOG(1, LogLevel::Info, LogCategory::Audio, 
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

    LOG(1, LogLevel::Info, LogCategory::Audio, 
        "[AudioProcessor] Processing thread finished.");
}