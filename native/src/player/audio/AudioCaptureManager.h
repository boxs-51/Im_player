#pragma once

#include <Windows.h>
#include <client.h>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <string>

#include "threads/thread_id.h"
#include "AudioTypes.h"
#include "SpscRingBuffer.h"

// Forward declaration
class PlayerStateSystem;

/**
 * @struct AudioFormatCache
 * @brief Cache local lưu cấu hình audio hiện tại để tránh lock PlayerStateSystem liên tục
 */
struct AudioFormatCache {
    uint32_t sampleRate = 48000;
    uint8_t channels = 2;
    AudioSampleFormat format = AudioSampleFormat::Float32;
};

/**
 * @class AudioCaptureManager
 * @brief Quản lý lấy dữ liệu PCM từ MPV qua Named Pipe, xử lý Overlapped I/O không gây nghẽn,
 * hỗ trợ tự động Reconnect và chuyển giao Lock-free sang SPSC RingBuffer.
 */
class AudioCaptureManager {
public:
    AudioCaptureManager();
    ~AudioCaptureManager();

    AudioCaptureManager(const AudioCaptureManager&) = delete;
    AudioCaptureManager& operator=(const AudioCaptureManager&) = delete;

    void Init(mpv_handle* mpv, PlayerStateSystem* stateSystem);
    void Shutdown();

    void StartCapture();
    void StopCapture();

    // Tín hiệu khi Seek / Đổi track
    void NotifySeekOrTrackChange();

    std::string GetPipeName() const;

    SpscRingBuffer<AudioBlock>& GetRawStream() { return m_rawAudioBuffer; }
    AudioPipelineMetrics GetMetrics() const { return m_metrics; }

private:
    void CaptureLoop();
    HANDLE CreateAudioPipe();
    void ClosePipeHandle(HANDLE hPipe);
    void UpdateFormatCacheFromState();

    // --- References & Handles ---
    mpv_handle* m_mpv = nullptr;
    PlayerStateSystem* m_stateSystem = nullptr;

    std::thread m_captureThread;
    std::atomic<bool> m_isRunning{false};
    std::atomic<bool> m_isCapturing{false};
    ThreadID m_threadId;

    // Lock-free Atomic Handle giúp UI Thread cancel I/O tức thì không cần Mutex
    std::atomic<HANDLE> m_atomicPipeHandle{INVALID_HANDLE_VALUE};
    std::string m_pipeName;

    // --- Local Caching & Metrics ---
    AudioFormatCache m_formatCache;
    std::atomic<uint64_t> m_currentGeneration{0};
    uint64_t m_sequence = 0;
    double m_currentPts = 0.0;
    AudioPipelineMetrics m_metrics;

    // --- Lock-free SPSC RingBuffer ---
    SpscRingBuffer<AudioBlock> m_rawAudioBuffer;
};