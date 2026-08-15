#include "Audio.h"
#include "PlayerStateSystem.h"
#include "log.h"

#include <iostream>

Audio::Audio() = default;

Audio::~Audio() {
    Shutdown();
}

bool Audio::Init(mpv_handle* mpv, PlayerStateSystem* stateSystem) {
    if (m_isInitialized.load(std::memory_order_relaxed)) {
        LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Audio, 
            std::cout << "[Audio] System is already initialized.");
        return true;
    }

    if (!mpv || !stateSystem) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
            std::cout << "[Audio] Init failed: mpv_handle or PlayerStateSystem is null.");
        return false;
    }

    // 1. Khởi tạo Capture Manager
    if (!m_audioCapture.Init(mpv, stateSystem)) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
            std::cout << "[Audio] Failed to initialize AudioCaptureManager.");
        return false;
    }

    // 2. Kết nối Audio Processor: Raw Stream (Capture) -> Processed Stream
    if (!m_audioProcessor.Init(&m_audioCapture.GetRawStream(), &m_processedAudioBuffer)) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
            std::cout << "[Audio] Failed to initialize AudioProcessor.");
        m_audioCapture.Shutdown();
        return false;
    }

    // 3. Kết nối Output Worker: Processed Stream -> Output Hardware Device (SDL2)
    if (!m_audioOutput.Init(&m_processedAudioBuffer, AudioBackendType::SDL2)) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
            std::cout << "[Audio] Failed to initialize AudioOutputWorker.");
        m_audioProcessor.Stop();
        m_audioCapture.Shutdown();
        return false;
    }

    // 4. Khai hỏa các Worker Threads theo đúng thứ tự
    m_audioProcessor.Start();
    m_audioOutput.Start();

    m_isInitialized.store(true, std::memory_order_release);
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
        std::cout << "[Audio] Pipeline successfully initialized and started.");

    return true;
}

void Audio::OnUserSeek() {
    if (!m_isInitialized.load(std::memory_order_relaxed)) return;

    // Tăng Generation ID. AudioOutputWorker & AudioCaptureManager sẽ tự dọn dẹp nội bộ
    // tuyệt đối KHÔNG can thiệp trực tiếp vào m_processedAudioBuffer tại đây.
    m_audioCapture.NotifySeekOrTrackChange(); 
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

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
        std::cout << "[Audio] Shutting down pipeline...");

    // Dừng theo thứ tự ngược lại của Pipeline: Consumer -> Intermediate -> Producer
    
    // 1. Dừng Output Worker (Luồng Consumer dừng hoàn toàn trước)
    m_audioOutput.Stop();

    // 2. Dừng Processor (Luồng Intermediate dừng)
    m_audioProcessor.Stop();

    // 3. Dừng Capture Manager (Luồng Producer dừng)
    m_audioCapture.Shutdown();

    // Sau khi toàn bộ các worker threads đã dừng (join), buffer an toàn để bỏ qua/hoàn tất mà không bị Data Race.

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
        std::cout << "[Audio] Pipeline safely shutdown.");
}