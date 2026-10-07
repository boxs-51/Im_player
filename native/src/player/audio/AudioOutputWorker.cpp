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
#include <cstdlib>

namespace {
std::uint32_t Issue31OutputPressureDelayMs() noexcept
{
    static const std::uint32_t delayMs = []() noexcept {
        const char* value = std::getenv("IM_PLAYER_AUDIO_OUTPUT_PRESSURE_DELAY_MS");
        if (!value || !*value)
            return 0u;

        char* end = nullptr;
        const unsigned long parsed = std::strtoul(value, &end, 10);
        if (end == value || (end && *end != '\0'))
            return 0u;

        return static_cast<std::uint32_t>(parsed > 1000UL ? 1000UL : parsed);
    }();
    return delayMs;
}
}

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
    const std::uint32_t issue31PressureDelayMs = Issue31OutputPressureDelayMs();
    if (issue31PressureDelayMs > 0) {
        EmitAudioTelemetryEvidence(
            "OUTPUT_PRESSURE_ARMED delay_ms=%u source=IM_PLAYER_AUDIO_OUTPUT_PRESSURE_DELAY_MS",
            issue31PressureDelayMs);
    }

    std::uint64_t lastSuccessfulWriteMicros = 0;
    std::uint64_t lastSuccessfulLoadId = 0;
    std::uint64_t lastSuccessfulSequence = 0;
    std::uint64_t lastSuccessfulGeneration = 0;
    double lastSuccessfulPts = 0.0;
    double lastSuccessfulEndPts = 0.0;

    // #38 production candidate: the canonical reservoir becoming ready is
    // necessary but not sufficient to start the independent SDL hardware
    // clock. The initial startup load additionally waits for a real monotonic
    // transition of MPV's playback clock.
    constexpr double kStartupClockTransitionEpsilon = 0.000001;
    constexpr size_t kStartupClockGateHeadroomReserveBlocks = 4;
    std::uint64_t startupClockGatePrimedLoadId = 0;
    std::uint64_t startupClockGateLoadId = 0;
    std::uint64_t startupClockGateCompletedLoadId = 0;
    std::uint64_t startupClockGateArmedMicros = 0;
    std::uint64_t startupClockGateLastEvidenceMicros = 0;
    double startupClockGateBaselineTimePos = 0.0;
    double startupClockGateBaselinePlaybackTime = 0.0;
    bool startupClockGateArmed = false;

    std::uint64_t startupPlaybackLoadId = 0;
    std::uint64_t startupPlaybackStartMicros = 0;
    bool startupCheckpoint1sEmitted = false;
    bool startupCheckpoint5sEmitted = false;
    bool startupCheckpoint10sEmitted = false;

    struct StartupPlaybackSnapshot {
        double timePos = 0.0;
        double playbackTime = 0.0;
        bool isPaused = true;
        bool isCoreIdle = true;
        bool isIdleActive = true;
        bool isSeeking = false;
        bool pauseForCache = false;
        int cacheBufferingState = 0;
    };

    const auto readStartupPlaybackSnapshot = [this]() {
        StartupPlaybackSnapshot snapshot;
        if (m_stateSystem) {
            m_stateSystem->ReadPlayback([&snapshot](const PlaybackModel& model) {
                snapshot.timePos = model.timing.timePos;
                snapshot.playbackTime = model.timing.playbackTime;
                snapshot.isPaused = model.flags.isPaused;
                snapshot.isCoreIdle = model.flags.isCoreIdle;
                snapshot.isIdleActive = model.flags.isIdleActive;
                snapshot.isSeeking = model.flags.isSeeking;
                snapshot.pauseForCache = model.flags.pauseForCache;
            });
            m_stateSystem->ReadNetwork([&snapshot](const NetworkModel& model) {
                snapshot.cacheBufferingState = model.cache_buffering_state;
            });
        }
        return snapshot;
    };

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

            if (issue31PressureDelayMs > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(issue31PressureDelayMs));
                if (!m_isRunning.load(std::memory_order_relaxed))
                    break;
            }

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
                    if (lastSuccessfulLoadId > 0 && AudioStartupBoundaryTraceEnabled()) {
                        const auto playbackSnapshot = readStartupPlaybackSnapshot();
                        EmitStartupBoundaryEvidence(
                            "stage=SDL_QUEUE_UNDERFLOW load_id=%llu t_us=%llu index=%llu empty_us=%llu queued_bytes=%u processed_ring_size=%zu last_write_us=%llu last_sequence=%llu last_generation=%llu last_pts=%.6f mpv_time_pos=%.6f playback_time=%.6f paused=%d core_idle=%d idle_active=%d seeking=%d pause_for_cache=%d cache_buffering_state=%d source=idle_observer",
                            static_cast<unsigned long long>(lastSuccessfulLoadId),
                            static_cast<unsigned long long>(queueObservationMicros),
                            static_cast<unsigned long long>(underflowIndex),
                            static_cast<unsigned long long>(queueDecision.emptyDurationMicros),
                            queuedBytes,
                            m_processedStream->size(),
                            static_cast<unsigned long long>(lastSuccessfulWriteMicros),
                            static_cast<unsigned long long>(lastSuccessfulSequence),
                            static_cast<unsigned long long>(lastSuccessfulGeneration),
                            lastSuccessfulPts,
                            playbackSnapshot.timePos,
                            playbackSnapshot.playbackTime,
                            playbackSnapshot.isPaused ? 1 : 0,
                            playbackSnapshot.isCoreIdle ? 1 : 0,
                            playbackSnapshot.isIdleActive ? 1 : 0,
                            playbackSnapshot.isSeeking ? 1 : 0,
                            playbackSnapshot.pauseForCache ? 1 : 0,
                            playbackSnapshot.cacheBufferingState);
                    }
                    m_audioDevice->FlushBuffers();
                    playbackStartedObserved = false;
                    underflowDetector.Reset();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                if (startupClockGateArmed &&
                    queuedBytes >= kAudioOutputTargetQueueBytes &&
                    startupClockGateLoadId > 0 &&
                    lastSuccessfulLoadId == startupClockGateLoadId) {
                    const auto playbackSnapshot = readStartupPlaybackSnapshot();
                    const size_t processedRingSize = m_processedStream->size();
                    const size_t processedRingCapacity = m_processedStream->capacity();
                    const size_t headroomBlocks =
                        processedRingCapacity > processedRingSize
                            ? processedRingCapacity - processedRingSize
                            : 0;

                    const bool clockTransitionObserved =
                        playbackSnapshot.timePos >
                            startupClockGateBaselineTimePos +
                                kStartupClockTransitionEpsilon &&
                        playbackSnapshot.playbackTime >
                            startupClockGateBaselinePlaybackTime +
                                kStartupClockTransitionEpsilon;

                    const bool livenessFallback =
                        processedRingCapacity > kStartupClockGateHeadroomReserveBlocks &&
                        processedRingSize >=
                            processedRingCapacity -
                                kStartupClockGateHeadroomReserveBlocks;

                    if (clockTransitionObserved || livenessFallback) {
                        auto* sdlDevice =
                            dynamic_cast<SdlAudioDevice*>(m_audioDevice.get());

                        if (livenessFallback && !clockTransitionObserved) {
                            EmitAudioTelemetryEvidence(
                                "ERROR STARTUP_CLOCK_RELEASE_LIVENESS_FALLBACK load_id=%llu processed_ring_size=%zu processed_ring_capacity=%zu headroom_blocks=%zu reason=ring_pressure",
                                static_cast<unsigned long long>(
                                    startupClockGateLoadId),
                                processedRingSize,
                                processedRingCapacity,
                                headroomBlocks);
                        }

                        if (sdlDevice &&
                            sdlDevice->StartPlaybackIfPrebuffered()) {
                            sdlDevice->SetAutoPlaybackStart(true);
                            playbackStartedObserved = true;
                            startupClockGateCompletedLoadId =
                                startupClockGateLoadId;
                            startupClockGateArmed = false;

                            startupPlaybackLoadId =
                                startupClockGateCompletedLoadId;
                            startupPlaybackStartMicros =
                                queueObservationMicros;
                            startupCheckpoint1sEmitted = false;
                            startupCheckpoint5sEmitted = false;
                            startupCheckpoint10sEmitted = false;

                            const double audibleHead =
                                EstimateAudibleHeadPts(
                                    lastSuccessfulEndPts,
                                    queuedBytes);
                            const double avOffset =
                                audibleHead - playbackSnapshot.timePos;

                            EmitStartupBoundaryEvidence(
                                "stage=STARTUP_CLOCK_RELEASE_GATE_GRANTED load_id=%llu t_us=%llu wait_us=%llu baseline_mpv_time_pos=%.6f baseline_playback_time=%.6f mpv_time_pos=%.6f playback_time=%.6f queued_bytes=%u queued_ms=%.3f processed_ring_size=%zu processed_ring_capacity=%zu headroom_blocks=%zu reason=%s",
                                static_cast<unsigned long long>(
                                    startupClockGateCompletedLoadId),
                                static_cast<unsigned long long>(
                                    queueObservationMicros),
                                static_cast<unsigned long long>(
                                    queueObservationMicros -
                                    startupClockGateArmedMicros),
                                startupClockGateBaselineTimePos,
                                startupClockGateBaselinePlaybackTime,
                                playbackSnapshot.timePos,
                                playbackSnapshot.playbackTime,
                                queuedBytes,
                                1000.0 *
                                    CanonicalQueuedAudioSeconds(queuedBytes),
                                processedRingSize,
                                processedRingCapacity,
                                headroomBlocks,
                                clockTransitionObserved
                                    ? "clock_transition"
                                    : "ring_pressure_fallback");

                            EmitStartupBoundaryEvidence(
                                "stage=SDL_PLAYBACK_STARTED load_id=%llu t_us=%llu sequence=%llu generation=%llu pts=%.6f queued_bytes=%u queued_ms=%.3f audible_head_pts=%.6f mpv_time_pos=%.6f playback_time=%.6f av_offset_s=%.6f paused=%d core_idle=%d idle_active=%d seeking=%d pause_for_cache=%d cache_buffering_state=%d release_gate=%s",
                                static_cast<unsigned long long>(
                                    startupPlaybackLoadId),
                                static_cast<unsigned long long>(
                                    queueObservationMicros),
                                static_cast<unsigned long long>(
                                    lastSuccessfulSequence),
                                static_cast<unsigned long long>(
                                    lastSuccessfulGeneration),
                                lastSuccessfulPts,
                                queuedBytes,
                                1000.0 *
                                    CanonicalQueuedAudioSeconds(queuedBytes),
                                audibleHead,
                                playbackSnapshot.timePos,
                                playbackSnapshot.playbackTime,
                                avOffset,
                                playbackSnapshot.isPaused ? 1 : 0,
                                playbackSnapshot.isCoreIdle ? 1 : 0,
                                playbackSnapshot.isIdleActive ? 1 : 0,
                                playbackSnapshot.isSeeking ? 1 : 0,
                                playbackSnapshot.pauseForCache ? 1 : 0,
                                playbackSnapshot.cacheBufferingState,
                                clockTransitionObserved
                                    ? "clock_transition"
                                    : "ring_pressure_fallback");
                        }
                    } else if (
                        startupClockGateLastEvidenceMicros == 0 ||
                        queueObservationMicros -
                            startupClockGateLastEvidenceMicros >= 100000ULL) {
                        startupClockGateLastEvidenceMicros =
                            queueObservationMicros;
                        EmitStartupBoundaryEvidence(
                            "stage=STARTUP_CLOCK_RELEASE_GATE_WAIT load_id=%llu t_us=%llu baseline_mpv_time_pos=%.6f baseline_playback_time=%.6f mpv_time_pos=%.6f playback_time=%.6f queued_bytes=%u queued_ms=%.3f processed_ring_size=%zu processed_ring_capacity=%zu headroom_blocks=%zu",
                            static_cast<unsigned long long>(
                                startupClockGateLoadId),
                            static_cast<unsigned long long>(
                                queueObservationMicros),
                            startupClockGateBaselineTimePos,
                            startupClockGateBaselinePlaybackTime,
                            playbackSnapshot.timePos,
                            playbackSnapshot.playbackTime,
                            queuedBytes,
                            1000.0 *
                                CanonicalQueuedAudioSeconds(queuedBytes),
                            processedRingSize,
                            processedRingCapacity,
                            headroomBlocks);
                    }
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

            const std::uint64_t outputAcquireMicros = AudioTelemetryNowMicros();
            const size_t processedRingSizeAtAcquire = m_processedStream->size();
            if (block->startupLoadId > 0 && AudioStartupBoundaryTraceEnabled()) {
                EmitStartupBoundaryEvidence(
                    "stage=OUTPUT_ACQUIRE_PROCESSED load_id=%llu t_us=%llu sequence=%llu generation=%llu pts=%.6f frames=%u processed_ring_size=%zu processed_ring_capacity=%zu capture_read_us=%llu capture_publish_us=%llu queued_bytes=%u",
                    static_cast<unsigned long long>(block->startupLoadId),
                    static_cast<unsigned long long>(outputAcquireMicros),
                    static_cast<unsigned long long>(block->sequence),
                    static_cast<unsigned long long>(block->generation),
                    block->pts,
                    block->frames,
                    processedRingSizeAtAcquire,
                    m_processedStream->capacity(),
                    static_cast<unsigned long long>(block->captureReadMicros),
                    static_cast<unsigned long long>(block->capturePublishMicros),
                    m_audioDevice ? m_audioDevice->GetQueuedSizeBytes() : 0);
            }

            const std::uint64_t processedBlockMicros = outputAcquireMicros;
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

            auto* startupSdlDevice =
                dynamic_cast<SdlAudioDevice*>(m_audioDevice.get());
            if (block->startupLoadId > 0 &&
                startupSdlDevice &&
                !startupSdlDevice->IsPlaybackStarted() &&
                startupClockGateCompletedLoadId != block->startupLoadId &&
                startupClockGatePrimedLoadId != block->startupLoadId) {
                startupClockGatePrimedLoadId = block->startupLoadId;
                startupSdlDevice->SetAutoPlaybackStart(false);
                EmitStartupBoundaryEvidence(
                    "stage=STARTUP_CLOCK_RELEASE_GATE_PRIMED load_id=%llu t_us=%llu sequence=%llu generation=%llu pts=%.6f authority=mpv_clock_transition",
                    static_cast<unsigned long long>(block->startupLoadId),
                    static_cast<unsigned long long>(outputAcquireMicros),
                    static_cast<unsigned long long>(block->sequence),
                    static_cast<unsigned long long>(block->generation),
                    block->pts);
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
                            if (block->startupLoadId > 0 && AudioStartupBoundaryTraceEnabled()) {
                                const auto playbackSnapshot = readStartupPlaybackSnapshot();
                                EmitStartupBoundaryEvidence(
                                    "stage=SDL_QUEUE_UNDERFLOW load_id=%llu t_us=%llu index=%llu sequence=%llu generation=%llu pts=%.6f empty_us=%llu queued_bytes=%u processed_ring_size=%zu last_write_us=%llu mpv_time_pos=%.6f playback_time=%.6f paused=%d core_idle=%d idle_active=%d seeking=%d pause_for_cache=%d cache_buffering_state=%d source=segment",
                                    static_cast<unsigned long long>(block->startupLoadId),
                                    static_cast<unsigned long long>(segmentObservationMicros),
                                    static_cast<unsigned long long>(underflowIndex),
                                    static_cast<unsigned long long>(block->sequence),
                                    static_cast<unsigned long long>(block->generation),
                                    block->pts,
                                    static_cast<unsigned long long>(segmentDecision.emptyDurationMicros),
                                    queuedBefore,
                                    m_processedStream->size(),
                                    static_cast<unsigned long long>(lastSuccessfulWriteMicros),
                                    playbackSnapshot.timePos,
                                    playbackSnapshot.playbackTime,
                                    playbackSnapshot.isPaused ? 1 : 0,
                                    playbackSnapshot.isCoreIdle ? 1 : 0,
                                    playbackSnapshot.isIdleActive ? 1 : 0,
                                    playbackSnapshot.isSeeking ? 1 : 0,
                                    playbackSnapshot.pauseForCache ? 1 : 0,
                                    playbackSnapshot.cacheBufferingState);
                            }
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

                        const std::uint64_t segmentWriteMicros =
                            AudioTelemetryNowMicros();
                        if (block->startupLoadId > 0 && AudioStartupBoundaryTraceEnabled()) {
                            EmitStartupBoundaryEvidence(
                                "stage=SDL_SEGMENT_WRITE load_id=%llu t_us=%llu sequence=%llu generation=%llu pts=%.6f frame_offset=%u frames=%u queued_before=%u queued_after=%u processed_ring_size=%zu",
                                static_cast<unsigned long long>(block->startupLoadId),
                                static_cast<unsigned long long>(segmentWriteMicros),
                                static_cast<unsigned long long>(block->sequence),
                                static_cast<unsigned long long>(block->generation),
                                block->pts,
                                framesQueuedFromBlock - writePlan.framesToWrite,
                                writePlan.framesToWrite,
                                queuedBefore,
                                finalQueuedBytes,
                                m_processedStream->size());
                        }

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
                            const bool semanticGateOwnsInitialRelease =
                                block->startupLoadId > 0 &&
                                startupClockGatePrimedLoadId ==
                                    block->startupLoadId &&
                                startupClockGateCompletedLoadId !=
                                    block->startupLoadId &&
                                startupSdlDevice &&
                                !startupSdlDevice->IsPlaybackStarted();

                            if (semanticGateOwnsInitialRelease) {
                                if (!startupClockGateArmed) {
                                    const auto playbackSnapshot =
                                        readStartupPlaybackSnapshot();
                                    startupClockGateArmed = true;
                                    startupClockGateLoadId =
                                        block->startupLoadId;
                                    startupClockGateArmedMicros =
                                        segmentWriteMicros;
                                    startupClockGateLastEvidenceMicros = 0;
                                    startupClockGateBaselineTimePos =
                                        playbackSnapshot.timePos;
                                    startupClockGateBaselinePlaybackTime =
                                        playbackSnapshot.playbackTime;

                                    EmitStartupBoundaryEvidence(
                                        "stage=STARTUP_CLOCK_RELEASE_GATE_ARM load_id=%llu t_us=%llu baseline_mpv_time_pos=%.6f baseline_playback_time=%.6f queued_bytes=%u queued_ms=%.3f processed_ring_size=%zu processed_ring_capacity=%zu",
                                        static_cast<unsigned long long>(
                                            startupClockGateLoadId),
                                        static_cast<unsigned long long>(
                                            segmentWriteMicros),
                                        startupClockGateBaselineTimePos,
                                        startupClockGateBaselinePlaybackTime,
                                        finalQueuedBytes,
                                        1000.0 *
                                            CanonicalQueuedAudioSeconds(
                                                finalQueuedBytes),
                                        m_processedStream->size(),
                                        m_processedStream->capacity());
                                }
                            } else {
                                playbackStartedObserved = true;

                                if (block->startupLoadId > 0) {
                                    startupPlaybackLoadId =
                                        block->startupLoadId;
                                    startupPlaybackStartMicros =
                                        segmentWriteMicros;
                                    startupCheckpoint1sEmitted = false;
                                    startupCheckpoint5sEmitted = false;
                                    startupCheckpoint10sEmitted = false;

                                    if (AudioStartupBoundaryTraceEnabled()) {
                                        const auto playbackSnapshot =
                                            readStartupPlaybackSnapshot();
                                        const double playbackStartMpvTimePos =
                                            playbackSnapshot.timePos;

                                        const double segmentEndPts =
                                            block->pts +
                                            static_cast<double>(
                                                framesQueuedFromBlock) /
                                                static_cast<double>(
                                                    kCanonicalAudioSampleRate);
                                        const double audibleHead =
                                            EstimateAudibleHeadPts(
                                                segmentEndPts,
                                                finalQueuedBytes);
                                        const double avOffset =
                                            audibleHead -
                                            playbackStartMpvTimePos;

                                        EmitStartupBoundaryEvidence(
                                            "stage=SDL_PLAYBACK_STARTED load_id=%llu t_us=%llu sequence=%llu generation=%llu pts=%.6f queued_bytes=%u queued_ms=%.3f audible_head_pts=%.6f mpv_time_pos=%.6f playback_time=%.6f av_offset_s=%.6f paused=%d core_idle=%d idle_active=%d seeking=%d pause_for_cache=%d cache_buffering_state=%d release_gate=legacy_reservoir",
                                            static_cast<unsigned long long>(
                                                block->startupLoadId),
                                            static_cast<unsigned long long>(
                                                segmentWriteMicros),
                                            static_cast<unsigned long long>(
                                                block->sequence),
                                            static_cast<unsigned long long>(
                                                block->generation),
                                            block->pts,
                                            finalQueuedBytes,
                                            1000.0 *
                                                CanonicalQueuedAudioSeconds(
                                                    finalQueuedBytes),
                                            audibleHead,
                                            playbackStartMpvTimePos,
                                            playbackSnapshot.playbackTime,
                                            avOffset,
                                            playbackSnapshot.isPaused ? 1 : 0,
                                            playbackSnapshot.isCoreIdle ? 1 : 0,
                                            playbackSnapshot.isIdleActive ? 1 : 0,
                                            playbackSnapshot.isSeeking ? 1 : 0,
                                            playbackSnapshot.pauseForCache
                                                ? 1
                                                : 0,
                                            playbackSnapshot.cacheBufferingState);
                                    }
                                }
                            }
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

                        lastSuccessfulWriteMicros = sdlWriteMicros;
                        lastSuccessfulLoadId = block->startupLoadId;
                        lastSuccessfulSequence = block->sequence;
                        lastSuccessfulGeneration = block->generation;
                        lastSuccessfulPts = block->pts;
                        lastSuccessfulEndPts = lastWrittenEndPts;

                        if (block->startupLoadId > 0 &&
                            block->startupLoadId == startupPlaybackLoadId &&
                            startupPlaybackStartMicros > 0 &&
                            AudioStartupBoundaryTraceEnabled()) {
                            const std::uint64_t elapsedMicros =
                                sdlWriteMicros - startupPlaybackStartMicros;

                            const auto emitCheckpoint =
                                [&](std::uint64_t checkpointSeconds, bool& emitted) {
                                    const std::uint64_t thresholdMicros =
                                        checkpointSeconds * 1'000'000ULL;
                                    if (!emitted && elapsedMicros >= thresholdMicros) {
                                        emitted = true;
                                        const auto playbackSnapshot =
                                            readStartupPlaybackSnapshot();
                                        EmitStartupBoundaryEvidence(
                                            "stage=STARTUP_SYNC_SAMPLE checkpoint_s=%llu load_id=%llu t_us=%llu elapsed_us=%llu sequence=%llu generation=%llu queued_bytes=%u queued_ms=%.3f last_written_end_pts=%.6f audible_head_pts=%.6f mpv_time_pos=%.6f playback_time=%.6f av_offset_s=%.6f processed_ring_size=%zu paused=%d core_idle=%d idle_active=%d seeking=%d pause_for_cache=%d cache_buffering_state=%d",
                                            static_cast<unsigned long long>(checkpointSeconds),
                                            static_cast<unsigned long long>(block->startupLoadId),
                                            static_cast<unsigned long long>(sdlWriteMicros),
                                            static_cast<unsigned long long>(elapsedMicros),
                                            static_cast<unsigned long long>(block->sequence),
                                            static_cast<unsigned long long>(block->generation),
                                            queuedBytes,
                                            1000.0 * CanonicalQueuedAudioSeconds(queuedBytes),
                                            lastWrittenEndPts,
                                            audibleHead,
                                            playbackSnapshot.timePos,
                                            playbackSnapshot.playbackTime,
                                            audibleHead - playbackSnapshot.timePos,
                                            m_processedStream->size(),
                                            playbackSnapshot.isPaused ? 1 : 0,
                                            playbackSnapshot.isCoreIdle ? 1 : 0,
                                            playbackSnapshot.isIdleActive ? 1 : 0,
                                            playbackSnapshot.isSeeking ? 1 : 0,
                                            playbackSnapshot.pauseForCache ? 1 : 0,
                                            playbackSnapshot.cacheBufferingState);
                                    }
                                };

                            emitCheckpoint(1, startupCheckpoint1sEmitted);
                            emitCheckpoint(5, startupCheckpoint5sEmitted);
                            emitCheckpoint(10, startupCheckpoint10sEmitted);
                        }

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