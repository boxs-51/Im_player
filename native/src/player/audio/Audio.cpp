#include "Audio.h"

void Audio::Init(mpv_handle* m_mpv, PlayerStateSystem* m_state) {
    // 1. Khởi tạo MPV & StateSystem như bình thường

    // 2. Khởi tạo Capture Manager (Tạo Pipe + Set option cho MPV)
    m_audioCapture.Init(m_mpv, m_state);

    // 3. Khởi tạo Audio Processor (Kết nối Raw Stream -> Processed Stream)
    m_audioProcessor.Init(&m_audioCapture.GetRawStream(), &m_processedAudioBuffer);

    // 4. Khởi tạo Audio Output Worker (Đọc dữ liệu từ Processed Stream)
    m_audioOutput.Init(&m_audioCapture, m_state); // Khởi tạo phần cứng SDL
    // Truyền buffer đã qua xử lý cho Output Worker
    // (Lưu ý: Bạn có thể cập nhật AudioOutputWorker::Init để nhận pointer đến m_processedAudioBuffer)

    // 5. Khai hỏa tất cả Worker Threads
    m_audioProcessor.Start();
    m_audioOutput.Start();
}

void Audio::OnUserSeek() {
    // Tăng generation ID & clear capture buffer
    m_audioCapture.NotifySeekOrTrackChange(); 

    // Clear buffer trung gian
    //m_processedAudioBuffer.clear();

}

void Audio::Shutdown() {
    // 1. Dừng Output Worker (Consumer)
    m_audioOutput.Stop();

    // 2. Dừng Processor (Intermediate)
    m_audioProcessor.Stop();

    // 3. Dừng Capture Manager (Producer)
    m_audioCapture.Shutdown();

    // 4. Reset/Clear sạch dữ liệu đệm dở dang trong RingBuffer
    AudioBlock dummyBlock;
    while (m_processedAudioBuffer.try_pop(dummyBlock)) {
        // Pop hết block thừa để giải phóng std::vector<float> samples bên trong
    }
}