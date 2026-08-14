#pragma once

#include <client.h>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include "threads/thread_id.h"
#include "ring_buffer.h"

static const char* RAW_AUDIO_CAPTURE_FILTER_NAME = "implayer-raw-capture";
// Forward declaration
class PlayerStateSystem;

/**
 * @class AudioCaptureManager
 * @brief Chịu trách nhiệm lấy dữ liệu âm thanh thô (PCM) trực tiếp từ pipeline của MPV.
 *
 * Lớp này hoạt động trên một luồng riêng, sử dụng các hook của MPV để "tap" vào
 * luồng audio, sau đó đẩy dữ liệu vào một ring buffer an toàn luồng. Các thành phần
 * khác (như bộ phân tích, visualizer) có thể đọc dữ liệu từ buffer này mà không
 * làm ảnh hưởng đến luồng render chính của MPV.
 */
class AudioCaptureManager {
public:
    AudioCaptureManager();
    ~AudioCaptureManager();

    // Cấm sao chép để đảm bảo ownership duy nhất
    AudioCaptureManager(const AudioCaptureManager&) = delete;
    AudioCaptureManager& operator=(const AudioCaptureManager&) = delete;

    void Init(mpv_handle* mpv, PlayerStateSystem* stateSystem);
    void Shutdown();

    void StartCapture();
    void StopCapture();

    // Hàm được gọi từ callback của MPV để xử lý dữ liệu audio
    void OnAudioData(void* mpv_data);

private:
    void CaptureLoop();

    // --- Thành viên quản lý luồng và trạng thái ---
    mpv_handle* m_mpv = nullptr;
    PlayerStateSystem* m_stateSystem = nullptr;

    std::thread m_captureThread;
    std::atomic<bool> m_isRunning{false};
    std::atomic<bool> m_isCapturing{false};
    ThreadID m_threadId;

    std::mutex m_mutex;
    std::condition_variable m_cv;

    // --- Bộ đệm dữ liệu ---
    // Dữ liệu âm thanh gốc, trước khi qua các bộ lọc của AudioFilterManager
    ThreadSafeRingBuffer<float> m_preFilterBuffer;

    // Dữ liệu âm thanh đã được xử lý, sau khi qua các bộ lọc
    ThreadSafeRingBuffer<float> m_postFilterBuffer;
};