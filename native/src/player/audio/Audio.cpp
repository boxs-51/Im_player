#include "Audio.h"
#include "PlayerStateSystem.h"

Audio::Audio() = default;

Audio::~Audio() {
    Shutdown();
}

bool Audio::Init(mpv_handle* mpv, PlayerStateSystem* stateSystem) {
    if (!mpv || !stateSystem) return false;

    // 1. Khởi tạo Capture Manager (Đăng ký Pipe / MPV audio filter stream)
    if (!m_audioCapture.Init(mpv, stateSystem)) {
        return false;
    }

    // 2. Kết nối Audio Processor: Raw Stream (Capture) -> Processed Stream
    if (!m_audioProcessor.Init(&m_audioCapture.GetRawStream(), &m_processedAudioBuffer)) {
        m_audioCapture.Shutdown();
        return false;
    }

    // 3. Kết nối Output Worker: Processed Stream -> Output Hardware Device (SDL2)
    if (!m_audioOutput.Init(&m_processedAudioBuffer, stateSystem, AudioBackendType::SDL2)) {
        m_audioProcessor.Stop();
        m_audioCapture.Shutdown();
        return false;
    }

    // 4. Khai hỏa các Worker Threads
    m_audioProcessor.Start();
    m_audioOutput.Start();

    return true;
}

void Audio::OnUserSeek() {
    // Tăng Generation ID để báo cho Output Worker xả phần cứng
    m_audioCapture.NotifySeekOrTrackChange(); 

    // Xả sạch tất cả các block rác còn tồn trong RingBuffer trung gian
    const AudioBlock* slot = nullptr;
    while ((slot = m_processedAudioBuffer.acquire_read()) != nullptr) {
        m_processedAudioBuffer.release_read();
    }
}

bool Audio::GetVisualizerData(AudioVisualizerFrame& outFrame) {
    return m_audioProcessor.GetLatestVisualizerData(outFrame);
}

void Audio::Shutdown() {
    // Dừng theo thứ tự ngược lại của Pipeline: Consumer -> Intermediate -> Producer
    
    // 1. Dừng Output Worker
    m_audioOutput.Stop();

    // 2. Dừng Processor
    m_audioProcessor.Stop();

    // 3. Dừng Capture Manager
    m_audioCapture.Shutdown();

    // 4. Dọn dẹp sạch RingBuffer đệm dở dang
    const AudioBlock* slot = nullptr;
    while ((slot = m_processedAudioBuffer.acquire_read()) != nullptr) {
        m_processedAudioBuffer.release_read();
    }
}