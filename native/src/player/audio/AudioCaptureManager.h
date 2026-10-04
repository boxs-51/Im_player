#pragma once

#include <Windows.h>
#include <mpv/client.h>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <string>
#include <optional>

#include "threads/thread_id.h"
#include "AudioTypes.h"
#include "SpscRingBuffer.h"

// Forward declaration
class PlayerStateSystem;

/**
 * @struct AudioPipelineMetricsAtomic
 * @brief Metrics được atomic hóa để UI Thread có thể đọc an toàn
 */
struct AudioPipelineMetricsAtomic {
    std::atomic<uint64_t> blocksReceived{0};
    std::atomic<uint64_t> blocksDropped{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<uint64_t> ringOverflows{0};
    std::atomic<uint64_t> backpressureWaits{0};
    std::atomic<uint64_t> pacingSleepCount{0};
    std::atomic<uint64_t> pacingSleepMicros{0};
    std::atomic<uint64_t> pacingRebases{0};
    std::atomic<uint64_t> lastSequence{0};
    std::atomic<uint64_t> currentGeneration{0};
    std::atomic<uint64_t> partialFrameCarryBytes{0};
    std::atomic<uint64_t> firstPipeConnectedMicros{0};
    std::atomic<uint64_t> firstPipeBytesMicros{0};
    std::atomic<uint64_t> firstCompletePcmBlockMicros{0};
    std::atomic<double> lastPTS{0.0};
};

/**
 * @class AudioCaptureManager
 * @brief Thread duy nhất sở hữu Capture Data Plane (Read Pipe -> Produce Raw Ring Buffer)
 */
class AudioCaptureManager {
public:
    AudioCaptureManager();
    ~AudioCaptureManager();

    AudioCaptureManager(const AudioCaptureManager&) = delete;
    AudioCaptureManager& operator=(const AudioCaptureManager&) = delete;

    bool Init(mpv_handle* mpv, PlayerStateSystem* stateSystem, SpscProducer<AudioBlock> producer);
    void Shutdown();

    void StartCapture();
    void StopCapture();


    std::string GetPipeName() const;

    // Đọc Snapshot Metrics thread-safe
    AudioPipelineMetrics GetMetrics() const;

private:
    void CaptureLoop();
    HANDLE CreateAudioPipe();
    void ClosePipeHandle(HANDLE hPipe);
    bool ConfigureMpvPcmTransport();
    bool SetRequiredMpvProperty(const char* name, const char* value);
    void LogEffectiveMpvProperty(const char* name) const;

    // --- References & Handles ---
    mpv_handle* m_mpv = nullptr;
    PlayerStateSystem* m_stateSystem = nullptr;

    std::thread m_captureThread;
    std::atomic<bool> m_isRunning{false};
    std::atomic<bool> m_isCapturing{false};
    ThreadID m_threadId = "";

    // Handle pipe được quản lý an toàn qua CancelIoEx
    std::atomic<HANDLE> m_atomicPipeHandle{INVALID_HANDLE_VALUE};
    std::string m_pipeName;

    // Yêu cầu chuyển generation từ UI thread (Thread-safe trigger)
    std::atomic<uint64_t> m_generationRequest{0};

    // --- Local state & metrics ---
    // CHỈ CaptureThread được phép đọc/ghi các biến state nội bộ này!
    uint64_t m_activeGeneration = 0;
    uint64_t m_sequence = 0;
    double m_currentPts = 0.0;
    bool m_wasSeeking = false;

    AudioPipelineMetricsAtomic m_metrics;

    // Handle ghi dữ liệu duy nhất
    std::optional<SpscProducer<AudioBlock>> m_producer;
};