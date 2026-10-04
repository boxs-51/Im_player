#include "AudioOutputWorker.h"
#include "SdlAudioDevice.h"
#include "AudioTelemetry.h"
#include "AudioQueueUnderflowDetector.h"
#include "AudioQueueWritePlanner.h"
#include "log.h"
#include "threads/thread_manager.h"
#include "common/LifecycleEvidence.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

AudioOutputWorker::AudioOutputWorker()
{
    LifecycleEvidence::Emit(
        "AudioOutputWorker",
        "CREATE",
        LifecycleEvidence::PointerIdentity(this));
}

AudioOutputWorker::~AudioOutputWorker() {
    Stop();
    LifecycleEvidence::Emit(
        "AudioOutputWorker",
        "DESTROY",
        LifecycleEvidence::PointerIdentity(this));
}

std::unique_ptr<IAudioOutputDevice> AudioOutputWorker::CreateDeviceBackend(AudioBackendType type) {
    switch (type) {
        case AudioBackendType::SDL2:
            return std::make_unique<SdlAudioDevice>();
            
        case AudioBackendType::WASAPI:
            LOG(1, LogLevel::Warning, LogCategory::Audio, 
                "[AudioOutputWorker] WASAPI backend not yet implemented, fallback to SDL2.");
            return std::make_unique<SdlAudioDevice>();
            
        default:
            LOG(1, LogLevel::Warning, LogCategory::Audio, 
                "[AudioOutputWorker] Unknown backend type, fallback to SDL2.");
            return std::make_unique<SdlAudioDevice>();
    }
}

bool AudioOutputWorker::Init(
    SpscConsumer<AudioBlock> processedStream,
    PlayerStateSystem* stateSystem,
    AudioBackendType backend) {

    //if (!processedStream) {
    //    LOG(1, LogLevel::Error, LogCategory::Audio, 
    //        std::cout << "[AudioOutputWorker] Init failed: processedStream pointer is null.");
    //    return false;
    //}

    if (!stateSystem) {
        LOG(1, LogLevel::Error, LogCategory::Audio,
            "[AudioOutputWorker] Init failed: PlayerStateSystem is null.");
        return false;
    }

    m_processedStream.emplace(std::move(processedStream));
    m_stateSystem = stateSystem;
    m_threadId = "AudioOutputWorker_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    m_currentBackendType = backend;
    m_audioDevice = CreateDeviceBackend(m_currentBackendType);

    if (!m_audioDevice || !m_audioDevice->Open(kCanonicalAudioSampleRate, kCanonicalAudioChannels)) {
        LOG(1, LogLevel::Error, LogCategory::Audio, 
            "[AudioOutputWorker] Failed to open Audio Device Backend.");
        return false;
    }

    return true;
}

bool AudioOutputWorker::SwitchBackend(AudioBackendType newBackend) {
    bool wasRunning = m_isRunning.load(std::memory_order_relaxed);
    
    if (wasRunning) {
        Stop();
    }

    m_currentBackendType = newBackend;
    m_audioDevice = CreateDeviceBackend(m_currentBackendType);
    bool success = m_audioDevice && m_audioDevice->Open(kCanonicalAudioSampleRate, kCanonicalAudioChannels);

    if (wasRunning && success) {
        Start();
    }
    return success;
}

AudioOutputMetrics AudioOutputWorker::GetMetrics() const {
    AudioOutputMetrics snapshot;
    snapshot.blocksWritten = m_metrics.blocksWritten.load(std::memory_order_relaxed);
    snapshot.blocksDroppedFormatMismatch =
        m_metrics.blocksDroppedFormatMismatch.load(std::memory_order_relaxed);
    snapshot.queueUnderflowEvents =
        m_metrics.queueUnderflowEvents.load(std::memory_order_relaxed);
    snapshot.writeFailures = m_metrics.writeFailures.load(std::memory_order_relaxed);
    snapshot.firstProcessedBlockMicros =
        m_metrics.firstProcessedBlockMicros.load(std::memory_order_relaxed);
    snapshot.firstSdlWriteMicros =
        m_metrics.firstSdlWriteMicros.load(std::memory_order_relaxed);
    snapshot.firstNonzeroQueueMicros =
        m_metrics.firstNonzeroQueueMicros.load(std::memory_order_relaxed);
    snapshot.queuedBytes = m_metrics.queuedBytes.load(std::memory_order_relaxed);
    snapshot.queueHighWaterBytes =
        m_metrics.queueHighWaterBytes.load(std::memory_order_relaxed);
    snapshot.queuedMilliseconds =
        m_metrics.queuedMilliseconds.load(std::memory_order_relaxed);
    snapshot.lastWrittenPts =
        m_metrics.lastWrittenPts.load(std::memory_order_relaxed);
    snapshot.lastWrittenEndPts =
        m_metrics.lastWrittenEndPts.load(std::memory_order_relaxed);
    snapshot.mpvTimePos =
        m_metrics.mpvTimePos.load(std::memory_order_relaxed);
    snapshot.estimatedAudibleHeadPts =
        m_metrics.estimatedAudibleHeadPts.load(std::memory_order_relaxed);
    snapshot.estimatedAvOffsetSeconds =
        m_metrics.estimatedAvOffsetSeconds.load(std::memory_order_relaxed);
    snapshot.syncSampleCount =
        m_metrics.syncSampleCount.load(std::memory_order_relaxed);
    snapshot.firstSyncSampleMicros =
        m_metrics.firstSyncSampleMicros.load(std::memory_order_relaxed);
    snapshot.lastSyncSampleMicros =
        m_metrics.lastSyncSampleMicros.load(std::memory_order_relaxed);
    snapshot.firstSyncAvOffsetSeconds =
        m_metrics.firstSyncAvOffsetSeconds.load(std::memory_order_relaxed);
    snapshot.lastSyncAvOffsetSeconds =
        m_metrics.lastSyncAvOffsetSeconds.load(std::memory_order_relaxed);
    snapshot.maxAbsAvOffsetSeconds =
        m_metrics.maxAbsAvOffsetSeconds.load(std::memory_order_relaxed);
    snapshot.sampleRate = kCanonicalAudioSampleRate;
    snapshot.channels = kCanonicalAudioChannels;
    return snapshot;
}

void AudioOutputWorker::Start() {
    if (m_isRunning.load(std::memory_order_relaxed)) return;

    m_isRunning.store(true, std::memory_order_release);
    m_workerThread = std::thread(&AudioOutputWorker::OutputLoop, this);
    GetThreadManager().Register(m_threadId, &m_workerThread);
    LifecycleEvidence::Emit("AudioOutputWorker", "START", LifecycleEvidence::PointerIdentity(this));
}

void AudioOutputWorker::Stop() {
    if (!m_isRunning.exchange(false, std::memory_order_acq_rel)) return;

    LifecycleEvidence::Emit("AudioOutputWorker", "STOP", LifecycleEvidence::PointerIdentity(this));
    if (m_workerThread.joinable()) {
        m_workerThread.join();
        LifecycleEvidence::Emit("AudioOutputWorker", "JOIN", LifecycleEvidence::PointerIdentity(this));
    }

    GetThreadManager().Unregister(m_threadId);

    if (m_audioDevice) {
        m_audioDevice->Close();
    }
}

void AudioOutputWorker::OutputLoop() {
    LOG(1, LogLevel::Info, LogCategory::Audio, 
        "[AudioOutputWorker] Resilient Output Loop started with Safe Single-Consumer Pattern.");

    std::vector<float> volumeAdjustedBuffer;
    uint32_t deviceErrorCount = 0;
    auto lastDeviceRetryTime = std::chrono::steady_clock::now();
    bool playbackStartedObserved = false;
    AudioQueueUnderflowDetector underflowDetector;
    std::uint64_t lastSyncEvidenceMicros = 0;
    std::uint64_t syncSampleIndex = 0;
    constexpr std::uint64_t kSyncEvidenceIntervalMicros = 5'000'000ULL;

    const auto updateQueueMetrics = [this](uint32_t queuedBytes) {
        m_metrics.queuedBytes.store(queuedBytes, std::memory_order_relaxed);

        const double queuedMs = 1000.0 * CanonicalQueuedAudioSeconds(queuedBytes);
        m_metrics.queuedMilliseconds.store(queuedMs, std::memory_order_relaxed);

        uint32_t previousHigh =
            m_metrics.queueHighWaterBytes.load(std::memory_order_relaxed);
        while (queuedBytes > previousHigh &&
               !m_metrics.queueHighWaterBytes.compare_exchange_weak(
                   previousHigh,
                   queuedBytes,
                   std::memory_order_relaxed,
                   std::memory_order_relaxed)) {
        }
    };

    while (m_isRunning.load(std::memory_order_relaxed)) {
        try {
            if (/*!m_processedStream ||*/ !m_isRunning.load(std::memory_order_relaxed)) break;

            // =========================================================================
            // 1. TỰ KHÔI PHỤC THIẾT BỊ PHẦN CỨNG (HARDWARE RECOVERY / AUTO-REINIT)
            // =========================================================================
            if (!m_audioDevice || !m_audioDevice->IsReady()) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDeviceRetryTime).count() > 1000) {
                    lastDeviceRetryTime = now;
                    LOG(1, LogLevel::Warning, LogCategory::Audio, 
                        "[AudioOutputWorker] Audio device unavailable. Attempting auto-recovery...");

                    if (m_audioDevice) {
                        m_audioDevice->Close();
                    }

                    m_audioDevice = CreateDeviceBackend(m_currentBackendType);
                    if (m_audioDevice && m_audioDevice->Open(kCanonicalAudioSampleRate, kCanonicalAudioChannels)) {
                        LOG(1, LogLevel::Info, LogCategory::Audio, 
                            "[AudioOutputWorker] Audio device auto-recovery SUCCESSFUL!");
                        deviceErrorCount = 0;
                    } else {
                        LOG(1, LogLevel::Error, LogCategory::Audio, 
                            "[AudioOutputWorker] Audio device auto-recovery failed. Will retry...");
                    }
                }

                // Xóa bớt block tồn đọng duy nhất từ luồng OutputWorker khi mất thiết bị
                if (const AudioBlock* dummy = m_processedStream->acquire_read()) {
                    m_processedStream->release_read();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // =========================================================================
            // 2. GIỮ SDL QUEUE QUANH BOUNDED RESERVOIR TARGET
            // =========================================================================
            if (m_audioDevice && m_audioDevice->IsReady()) {
                const uint32_t queuedBytes = m_audioDevice->GetQueuedSizeBytes();
                updateQueueMetrics(queuedBytes);

                const std::uint64_t queueObservationMicros =
                    AudioTelemetryNowMicros();
                const AudioQueueEmptyDecision queueDecision =
                    underflowDetector.Observe(
                        playbackStartedObserved,
                        queuedBytes,
                        queueObservationMicros);

                if (queueDecision.event == AudioQueueEmptyEvent::Observed) {
                    EmitAudioTelemetryEvidence(
                        "SDL_QUEUE_EMPTY_OBSERVED t_us=%llu grace_us=%llu",
                        static_cast<unsigned long long>(queueObservationMicros),
                        static_cast<unsigned long long>(
                            kAudioOutputUnderflowGraceMicros));
                } else if (
                    queueDecision.event == AudioQueueEmptyEvent::Recovered) {
                    EmitAudioTelemetryEvidence(
                        "SDL_QUEUE_EMPTY_RECOVERED t_us=%llu empty_us=%llu",
                        static_cast<unsigned long long>(queueObservationMicros),
                        static_cast<unsigned long long>(
                            queueDecision.emptyDurationMicros));
                } else if (
                    queueDecision.event ==
                    AudioQueueEmptyEvent::SustainedUnderflow) {
                    const uint64_t underflowIndex =
                        m_metrics.queueUnderflowEvents.fetch_add(
                            1,
                            std::memory_order_relaxed) + 1;
                    EmitAudioTelemetryEvidence(
                        "ERROR SDL_QUEUE_UNDERFLOW index=%llu empty_us=%llu grace_us=%llu action=rearm_prebuffer",
                        static_cast<unsigned long long>(underflowIndex),
                        static_cast<unsigned long long>(
                            queueDecision.emptyDurationMicros),
                        static_cast<unsigned long long>(
                            kAudioOutputUnderflowGraceMicros));
                    m_audioDevice->FlushBuffers();
                    playbackStartedObserved = false;
                    underflowDetector.Reset();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                if (queuedBytes >= kAudioOutputTargetQueueBytes) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
            }

            // =========================================================================
            // 3. TRUY CẤP RINGBUFFER DÀNH RIÊNG CHO CONSUMER & XỬ LÝ GENERATION (SEEK)
            // =========================================================================
            const AudioBlock* block = m_processedStream->acquire_read();

            if (!block) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (block->samples.empty() || block->sample_count() == 0) {
                m_processedStream->release_read();
                continue;
            }

            const std::uint64_t processedBlockMicros = AudioTelemetryNowMicros();
            if (RecordFirstAudioTelemetry(
                    m_metrics.firstProcessedBlockMicros,
                    processedBlockMicros)) {
                LOG(1, LogLevel::Info, LogCategory::Audio,
                    "[AUDIO_TIMELINE] FIRST_PROCESSED_BLOCK t_us=%llu pts=%.6f frames=%u",
                    static_cast<unsigned long long>(processedBlockMicros),
                    block->pts,
                    block->frames);
                EmitAudioTelemetryEvidence(
                    "FIRST_PROCESSED_BLOCK t_us=%llu pts=%.6f frames=%u",
                    static_cast<unsigned long long>(processedBlockMicros),
                    block->pts,
                    block->frames);
            }

            if (block->format.sampleRate != kCanonicalAudioSampleRate ||
                block->format.channels != kCanonicalAudioChannels ||
                block->format.format != AudioSampleFormat::Float32) {
                m_metrics.blocksDroppedFormatMismatch.fetch_add(1, std::memory_order_relaxed);
                LOG(1, LogLevel::Error, LogCategory::Audio,
                    "[AudioOutputWorker] Dropping block with non-canonical format: rate=%u channels=%u format=%u",
                    block->format.sampleRate,
                    static_cast<unsigned int>(block->format.channels),
                    static_cast<unsigned int>(block->format.format));
                EmitAudioTelemetryEvidence(
                    "ERROR FORMAT_MISMATCH rate=%u channels=%u format=%u",
                    block->format.sampleRate,
                    static_cast<unsigned int>(block->format.channels),
                    static_cast<unsigned int>(block->format.format));
                m_processedStream->release_read();
                continue;
            }

            // --- KIỂM TRA GENERATION KHI SEEK (SINGLE-CONSUMER DRAIN) ---
            if (block->generation < m_lastGeneration) {
                // Bỏ qua block lỗi thời tạo ra trước thời điểm Seek
                m_processedStream->release_read();
                continue;
            }
            if (block->generation > m_lastGeneration) {
                // Phát hiện Seek mới -> Cập nhật Generation và xả sạch phần cứng âm thanh ngay lập tức
                m_lastGeneration = block->generation;
                
                if (m_audioDevice) {
                    m_audioDevice->FlushBuffers();
                }
                playbackStartedObserved = false;
                underflowDetector.Reset();
            }

            bool writePerformed = false;
            if (m_audioDevice && m_audioDevice->IsReady()) {

                //while (m_isRunning.load(std::memory_order_relaxed)) {
                //    if (m_processedStream->peek_generation() > m_lastGeneration) {
                //        break;
                //    }
                //    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                //}

                if (m_isRunning.load(std::memory_order_relaxed)) {
                    std::uint32_t framesQueuedFromBlock = 0;
                    std::uint32_t finalQueuedBytes =
                        m_audioDevice->GetQueuedSizeBytes();
                    bool segmentWriteFailed = false;

                    while (
                        framesQueuedFromBlock < block->frames &&
                        m_isRunning.load(std::memory_order_relaxed) &&
                        m_audioDevice &&
                        m_audioDevice->IsReady()) {
                        const std::uint32_t queuedBefore =
                            m_audioDevice->GetQueuedSizeBytes();
                        updateQueueMetrics(queuedBefore);

                        const std::uint64_t segmentObservationMicros =
                            AudioTelemetryNowMicros();
                        const AudioQueueEmptyDecision segmentDecision =
                            underflowDetector.Observe(
                                playbackStartedObserved,
                                queuedBefore,
                                segmentObservationMicros);

                        if (segmentDecision.event ==
                            AudioQueueEmptyEvent::Observed) {
                            EmitAudioTelemetryEvidence(
                                "SDL_QUEUE_EMPTY_OBSERVED t_us=%llu grace_us=%llu source=segment",
                                static_cast<unsigned long long>(
                                    segmentObservationMicros),
                                static_cast<unsigned long long>(
                                    kAudioOutputUnderflowGraceMicros));
                        } else if (
                            segmentDecision.event ==
                            AudioQueueEmptyEvent::Recovered) {
                            EmitAudioTelemetryEvidence(
                                "SDL_QUEUE_EMPTY_RECOVERED t_us=%llu empty_us=%llu source=segment",
                                static_cast<unsigned long long>(
                                    segmentObservationMicros),
                                static_cast<unsigned long long>(
                                    segmentDecision.emptyDurationMicros));
                        } else if (
                            segmentDecision.event ==
                            AudioQueueEmptyEvent::SustainedUnderflow) {
                            const uint64_t underflowIndex =
                                m_metrics.queueUnderflowEvents.fetch_add(
                                    1,
                                    std::memory_order_relaxed) + 1;
                            EmitAudioTelemetryEvidence(
                                "ERROR SDL_QUEUE_UNDERFLOW index=%llu empty_us=%llu grace_us=%llu action=rearm_prebuffer source=segment",
                                static_cast<unsigned long long>(
                                    underflowIndex),
                                static_cast<unsigned long long>(
                                    segmentDecision.emptyDurationMicros),
                                static_cast<unsigned long long>(
                                    kAudioOutputUnderflowGraceMicros));
                            m_audioDevice->FlushBuffers();
                            playbackStartedObserved = false;
                            underflowDetector.Reset();
                            finalQueuedBytes = 0;
                        }

                        const std::uint32_t remainingFrames =
                            block->frames - framesQueuedFromBlock;
                        const AudioQueueWritePlan writePlan =
                            PlanAudioQueueWrite(
                                finalQueuedBytes =
                                    m_audioDevice->GetQueuedSizeBytes(),
                                remainingFrames);

                        if (writePlan.framesToWrite == 0) {
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(1));
                            continue;
                        }

                        const std::size_t sampleOffset =
                            static_cast<std::size_t>(
                                framesQueuedFromBlock) *
                            kCanonicalAudioChannels;
                        const std::size_t samplesToWrite =
                            static_cast<std::size_t>(
                                writePlan.framesToWrite) *
                            kCanonicalAudioChannels;

                        m_audioDevice->Write(
                            block->samples.data() + sampleOffset,
                            samplesToWrite);
                        if (!m_audioDevice->IsReady()) {
                            segmentWriteFailed = true;
                            break;
                        }

                        framesQueuedFromBlock +=
                            writePlan.framesToWrite;
                        finalQueuedBytes =
                            m_audioDevice->GetQueuedSizeBytes();
                        updateQueueMetrics(finalQueuedBytes);

                        if (finalQueuedBytes >
                            kAudioOutputDesignMaxQueueBytes) {
                            EmitAudioTelemetryEvidence(
                                "ERROR SDL_QUEUE_HARD_CAP_EXCEEDED queued_bytes=%u hard_cap_bytes=%u",
                                finalQueuedBytes,
                                kAudioOutputDesignMaxQueueBytes);
                            segmentWriteFailed = true;
                            break;
                        }

                        if (!playbackStartedObserved &&
                            finalQueuedBytes >=
                                kAudioOutputTargetQueueBytes) {
                            playbackStartedObserved = true;
                        }

                        const AudioQueueEmptyDecision refillDecision =
                            underflowDetector.Observe(
                                playbackStartedObserved,
                                finalQueuedBytes,
                                AudioTelemetryNowMicros());
                        if (refillDecision.event ==
                            AudioQueueEmptyEvent::Recovered) {
                            EmitAudioTelemetryEvidence(
                                "SDL_QUEUE_EMPTY_RECOVERED empty_us=%llu source=write",
                                static_cast<unsigned long long>(
                                    refillDecision.emptyDurationMicros));
                        }
                    }

                    writePerformed =
                        !segmentWriteFailed &&
                        framesQueuedFromBlock == block->frames &&
                        m_audioDevice &&
                        m_audioDevice->IsReady();

                    if (writePerformed) {
                        m_metrics.blocksWritten.fetch_add(1, std::memory_order_relaxed);

                        const double lastWrittenPts = block->pts;
                        const double lastWrittenEndPts =
                            block->pts + block->duration_seconds();
                        const uint32_t queuedBytes = finalQueuedBytes;

                        double mpvTimePos = 0.0;
                        if (m_stateSystem) {
                            m_stateSystem->ReadPlayback([&mpvTimePos](const PlaybackModel& model) {
                                mpvTimePos = model.timing.timePos;
                            });
                        }

                        const double audibleHead =
                            EstimateAudibleHeadPts(lastWrittenEndPts, queuedBytes);
                        const double avOffset =
                            EstimateAudioVideoOffsetSeconds(
                                lastWrittenEndPts,
                                queuedBytes,
                                mpvTimePos);

                        m_metrics.lastWrittenPts.store(lastWrittenPts, std::memory_order_relaxed);
                        m_metrics.lastWrittenEndPts.store(lastWrittenEndPts, std::memory_order_relaxed);
                        m_metrics.mpvTimePos.store(mpvTimePos, std::memory_order_relaxed);
                        m_metrics.estimatedAudibleHeadPts.store(audibleHead, std::memory_order_relaxed);
                        m_metrics.estimatedAvOffsetSeconds.store(avOffset, std::memory_order_relaxed);

                        const double absAvOffset = std::fabs(avOffset);
                        double previousMax =
                            m_metrics.maxAbsAvOffsetSeconds.load(std::memory_order_relaxed);
                        while (absAvOffset > previousMax &&
                               !m_metrics.maxAbsAvOffsetSeconds.compare_exchange_weak(
                                   previousMax,
                                   absAvOffset,
                                   std::memory_order_relaxed,
                                   std::memory_order_relaxed)) {
                        }

                        const std::uint64_t sdlWriteMicros = AudioTelemetryNowMicros();
                        if (RecordFirstAudioTelemetry(
                                m_metrics.firstSdlWriteMicros,
                                sdlWriteMicros)) {
                            LOG(1, LogLevel::Info, LogCategory::Audio,
                                "[AUDIO_TIMELINE] FIRST_SDL_WRITE t_us=%llu pts=%.6f end_pts=%.6f",
                                static_cast<unsigned long long>(sdlWriteMicros),
                                lastWrittenPts,
                                lastWrittenEndPts);
                            EmitAudioTelemetryEvidence(
                                "FIRST_SDL_WRITE t_us=%llu pts=%.6f end_pts=%.6f",
                                static_cast<unsigned long long>(sdlWriteMicros),
                                lastWrittenPts,
                                lastWrittenEndPts);
                        }

                        if (lastSyncEvidenceMicros == 0 ||
                            sdlWriteMicros - lastSyncEvidenceMicros >=
                                kSyncEvidenceIntervalMicros) {
                            ++syncSampleIndex;
                            lastSyncEvidenceMicros = sdlWriteMicros;

                            if (syncSampleIndex == 1) {
                                m_metrics.firstSyncSampleMicros.store(
                                    sdlWriteMicros,
                                    std::memory_order_relaxed);
                                m_metrics.firstSyncAvOffsetSeconds.store(
                                    avOffset,
                                    std::memory_order_relaxed);
                            }

                            m_metrics.syncSampleCount.store(
                                syncSampleIndex,
                                std::memory_order_relaxed);
                            m_metrics.lastSyncSampleMicros.store(
                                sdlWriteMicros,
                                std::memory_order_relaxed);
                            m_metrics.lastSyncAvOffsetSeconds.store(
                                avOffset,
                                std::memory_order_relaxed);

                            EmitAudioTelemetryEvidence(
                                "SYNC_SAMPLE index=%llu t_us=%llu queued_bytes=%u queued_ms=%.3f last_written_end_pts=%.6f audible_head_pts=%.6f mpv_time_pos=%.6f av_offset_s=%.6f",
                                static_cast<unsigned long long>(syncSampleIndex),
                                static_cast<unsigned long long>(sdlWriteMicros),
                                queuedBytes,
                                1000.0 * CanonicalQueuedAudioSeconds(queuedBytes),
                                lastWrittenEndPts,
                                audibleHead,
                                mpvTimePos,
                                avOffset);
                        }

                        if (queuedBytes > 0) {
                            const std::uint64_t nonzeroQueueMicros = AudioTelemetryNowMicros();
                            if (RecordFirstAudioTelemetry(
                                    m_metrics.firstNonzeroQueueMicros,
                                    nonzeroQueueMicros)) {
                                LOG(1, LogLevel::Info, LogCategory::Audio,
                                    "[AUDIO_TIMELINE] FIRST_NONZERO_SDL_QUEUE t_us=%llu queued_bytes=%u queued_ms=%.3f last_written_pts=%.6f last_written_end_pts=%.6f audible_head_pts=%.6f mpv_time_pos=%.6f av_offset_s=%.6f",
                                    static_cast<unsigned long long>(nonzeroQueueMicros),
                                    queuedBytes,
                                    1000.0 * CanonicalQueuedAudioSeconds(queuedBytes),
                                    lastWrittenPts,
                                    lastWrittenEndPts,
                                    audibleHead,
                                    mpvTimePos,
                                    avOffset);
                                EmitAudioTelemetryEvidence(
                                    "FIRST_NONZERO_SDL_QUEUE t_us=%llu queued_bytes=%u queued_ms=%.3f last_written_pts=%.6f last_written_end_pts=%.6f audible_head_pts=%.6f mpv_time_pos=%.6f av_offset_s=%.6f",
                                    static_cast<unsigned long long>(nonzeroQueueMicros),
                                    queuedBytes,
                                    1000.0 * CanonicalQueuedAudioSeconds(queuedBytes),
                                    lastWrittenPts,
                                    lastWrittenEndPts,
                                    audibleHead,
                                    mpvTimePos,
                                    avOffset);
                            }
                        }
                    }
                }
            }

            if (!writePerformed) {
                m_metrics.writeFailures.fetch_add(1, std::memory_order_relaxed);
                deviceErrorCount++;
                if (deviceErrorCount > 10) {
                    LOG(1, LogLevel::Error, LogCategory::Audio, 
                        "[AudioOutputWorker] Hardware write failed repeatedly. Marking device as offline.");
                    if (m_audioDevice) {
                        m_audioDevice->SetReady(false);
                    }
                }
            } else {
                deviceErrorCount = 0;
            }

            m_processedStream->release_read();

        } catch (const std::exception& e) {
            LOG(1, LogLevel::Error, LogCategory::Audio, 
                "[AudioOutputWorker] Exception caught in loop: %s", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } catch (...) {
            LOG(1, LogLevel::Error, LogCategory::Audio, 
                "[AudioOutputWorker] Unknown crash prevented in loop.");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    LOG(1, LogLevel::Info, LogCategory::Audio, "[AudioOutputWorker] Output thread safely stopped.");
}