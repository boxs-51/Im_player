#include "Audio.h"
#include "AudioTelemetry.h"
#include "PlayerStateSystem.h"
#include "log.h"

#include <iostream>

Audio::Audio() = default;

Audio::~Audio() {
    Shutdown();
}

bool Audio::Init(mpv_handle* mpv, PlayerStateSystem* stateSystem) {
    if (m_isInitialized.load(std::memory_order_relaxed)) {
        LOG(1, LogLevel::Warning, LogCategory::Audio, 
            "[Audio] System is already initialized.");
        return true;
    }

    if (!mpv || !stateSystem) {
        LOG(1, LogLevel::Error, LogCategory::Audio, 
            "[Audio] Init failed: mpv_handle or PlayerStateSystem is null.");
        return false;
    }

    // 1. Tạo Ring Buffers và tách cặp Producer/Consumer
    // Raw Stream: 8 blocks
    auto [rawProducer, rawConsumer] = make_spsc_ring_buffer<AudioBlock>(8);
    
    // Processed Stream: 32 blocks
    auto [procProducer, procConsumer] = make_spsc_ring_buffer<AudioBlock>(32);

    // 2. Kết nối Audio Capture Manager (Chỉ cấp quyền GHI vào Raw Stream)
    if (!m_audioCapture.Init(mpv, stateSystem, std::move(rawProducer))) {
        LOG(1, LogLevel::Error, LogCategory::Audio, 
             "[Audio] Failed to initialize AudioCaptureManager.");
        return false;
    }

    // 3. Kết nối Audio Processor (ĐỌC từ Raw Stream, GHI vào Processed Stream)
    if (!m_audioProcessor.Init(std::move(rawConsumer), std::move(procProducer))) {
        LOG(1, LogLevel::Error, LogCategory::Audio, 
            "[Audio] Failed to initialize AudioProcessor.");
        m_audioCapture.Shutdown();
        return false;
    }

    // 4. Kết nối Output Worker (Chỉ cấp quyền ĐỌC từ Processed Stream)
    if (!m_audioOutput.Init(std::move(procConsumer), stateSystem, AudioBackendType::SDL2)) {
        LOG(1, LogLevel::Error, LogCategory::Audio, 
            "[Audio] Failed to initialize AudioOutputWorker.");
        m_audioProcessor.Stop();
        m_audioCapture.Shutdown();
        return false;
    }

    // 4. Khai hỏa các Worker Threads theo đúng thứ tự
    m_audioProcessor.Start();
    m_audioOutput.Start();

    m_isInitialized.store(true, std::memory_order_release);
    LOG(1, LogLevel::Info, LogCategory::Audio, 
        "[Audio] Pipeline successfully initialized and started.");

    return true;
}

bool Audio::GetVisualizerData(AudioVisualizerFrame& outFrame) {
    if (!m_isInitialized.load(std::memory_order_relaxed)) return false;
    return m_audioProcessor.GetLatestVisualizerData(outFrame);
}

bool Audio::SwitchBackend(AudioBackendType newBackend) {
    if (!m_isInitialized.load(std::memory_order_relaxed)) return false;
    return m_audioOutput.SwitchBackend(newBackend);
}

void Audio::Shutdown() {
    if (!m_isInitialized.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    LOG(1, LogLevel::Info, LogCategory::Audio, 
        "[Audio] Shutting down pipeline...");

    const AudioPipelineMetrics captureMetrics = m_audioCapture.GetMetrics();
    const AudioOutputMetrics outputMetrics = m_audioOutput.GetMetrics();
    EmitAudioTelemetryEvidence(
        "SUMMARY capture_blocks=%llu capture_dropped=%llu capture_bytes=%llu ring_overflows=%llu backpressure_waits=%llu pacing_sleeps=%llu pacing_sleep_us=%llu pacing_rebases=%llu carry_bytes=%llu generation=%llu output_blocks=%llu format_mismatch=%llu underflows=%llu write_failures=%llu queued_bytes=%u queued_ms=%.3f queue_high_water_bytes=%u last_written_end_pts=%.6f mpv_time_pos=%.6f audible_head_pts=%.6f av_offset_s=%.6f",
        static_cast<unsigned long long>(captureMetrics.blocksReceived),
        static_cast<unsigned long long>(captureMetrics.blocksDropped),
        static_cast<unsigned long long>(captureMetrics.bytesReceived),
        static_cast<unsigned long long>(captureMetrics.ringOverflows),
        static_cast<unsigned long long>(captureMetrics.backpressureWaits),
        static_cast<unsigned long long>(captureMetrics.pacingSleepCount),
        static_cast<unsigned long long>(captureMetrics.pacingSleepMicros),
        static_cast<unsigned long long>(captureMetrics.pacingRebases),
        static_cast<unsigned long long>(captureMetrics.partialFrameCarryBytes),
        static_cast<unsigned long long>(captureMetrics.currentGeneration),
        static_cast<unsigned long long>(outputMetrics.blocksWritten),
        static_cast<unsigned long long>(outputMetrics.blocksDroppedFormatMismatch),
        static_cast<unsigned long long>(outputMetrics.queueUnderflowEvents),
        static_cast<unsigned long long>(outputMetrics.writeFailures),
        outputMetrics.queuedBytes,
        outputMetrics.queuedMilliseconds,
        outputMetrics.queueHighWaterBytes,
        outputMetrics.lastWrittenEndPts,
        outputMetrics.mpvTimePos,
        outputMetrics.estimatedAudibleHeadPts,
        outputMetrics.estimatedAvOffsetSeconds);

    // Dừng theo thứ tự ngược lại của Pipeline: Consumer -> Intermediate -> Producer
    
    // 1. Dừng Output Worker (Luồng Consumer dừng hoàn toàn trước)
    m_audioOutput.Stop();

    // 2. Dừng Processor (Luồng Intermediate dừng)
    m_audioProcessor.Stop();

    // 3. Dừng Capture Manager (Luồng Producer dừng)
    m_audioCapture.Shutdown();

    // Sau khi toàn bộ các worker threads đã dừng (join), buffer an toàn để bỏ qua/hoàn tất mà không bị Data Race.

    LOG(1, LogLevel::Info, LogCategory::Audio, 
        "[Audio] Pipeline safely shutdown.");
}