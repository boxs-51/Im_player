#include "AudioCaptureManager.h"
#include "log.h"
#include "threads/thread_manager.h"
#include <client.h>
#include <mpv/render.h>
#include <iostream>


AudioCaptureManager::AudioCaptureManager() 
    // Khởi tạo ring buffer với dung lượng 48000 samples (tương đương 1 giây ở 48kHz)
    : m_preFilterBuffer(48000), m_postFilterBuffer(48000), m_threadId("")
{}


AudioCaptureManager::~AudioCaptureManager() {
    // Destructor: Đảm bảo Shutdown được gọi để dọn dẹp tài nguyên
    Shutdown();
}

void AudioCaptureManager::Init(mpv_handle* mpv, PlayerStateSystem* stateSystem) {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioCaptureManager] Initializing...");
    m_mpv = mpv;
    m_stateSystem = stateSystem;
    m_threadId = "AudioCaptureManager_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    StartCapture();

    // Đăng ký một hook vào pipeline âm thanh của MPV.
    // MPV sẽ gọi `on_audio_data_callback` mỗi khi có một khối dữ liệu audio mới.
    // Chúng ta sẽ nhận sự kiện MPV_EVENT_HOOK với reply_userdata là AUDIO_CAPTURE_HOOK_ID.
    // Priority = 0: Mức ưu tiên trung bình.
    // "audio_output": Tên của hook point.

    // TODO: Quan sát thuộc tính audio-reconfig để xử lý khi sample rate/format thay đổi.
    // mpv_observe_property(m_mpv, 0, "audio-reconfig", MPV_FORMAT_NONE);
}

void AudioCaptureManager::Shutdown() {
    StopCapture();
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioCaptureManager] Shutdown complete.");
}

void AudioCaptureManager::StartCapture() {
    if (m_isRunning) {
        m_isCapturing = true;
        return;
    }

    m_isRunning = true;
    m_isCapturing = true;
    m_captureThread = std::thread(&AudioCaptureManager::CaptureLoop, this);
    GetThreadManager().Register(m_threadId, &m_captureThread);
}

void AudioCaptureManager::StopCapture() {
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    m_isCapturing = false;
    m_cv.notify_one(); // Đánh thức luồng nếu nó đang chờ

    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }

    GetThreadManager().Unregister(m_threadId);
}

void AudioCaptureManager::CaptureLoop() {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioCaptureManager] Capture thread started.");

    while (m_isRunning) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_isCapturing || !m_isRunning; });
        }

        if (!m_isRunning) break;

        // Luồng này hiện tại chỉ để "sống", việc nhận dữ liệu được thực hiện
        // trực tiếp trong luồng audio của MPV thông qua callback.
        // Trong tương lai, luồng này có thể được dùng để xử lý/phân tích dữ liệu
        // đã được đẩy vào ring buffer.
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Tạm thời nghỉ để tránh busy-loop
    }

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioCaptureManager] Capture thread finished.");
}

void AudioCaptureManager::OnAudioData(void* data) {
    /*
    if (!m_isCapturing || !data) {
        return;
    }

    mpv_frame* frame = static_cast<mpv_frame*>(data);

    // Chỉ xử lý frame audio
    if (frame->type != MPV_FRAME_TYPE_AUDIO) {
        return;
    }

    // Lấy thông tin và dữ liệu từ frame
    int samples = mpv_frame_get_metad(frame, "samples", 0);
    const char* format = mpv_frame_get_metad(frame, "format", "s16"); // Mặc định là s16 nếu không có
    void** frame_data = (void**)mpv_frame_get_data(frame);
    
    if (samples <= 0 || !frame_data || strcmp(format, "s16") != 0) {
        return;
    }

    int16_t* pcm_data = static_cast<int16_t*>(frame_data[0]);

    // Đẩy dữ liệu vào ring buffer
    for (int i = 0; i < samples; i++) {
        // Chuyển đổi từ int16_t [-32768, 32767] sang float [-1.0, 1.0]
        float float_sample = static_cast<float>(pcm_data[i]) / 32768.0f;
        m_preFilterBuffer.try_push(float_sample); // Sử dụng try_push để không block luồng audio của MPV
    }*/
}