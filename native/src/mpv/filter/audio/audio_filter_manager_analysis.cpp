#include "audio_filter_manager.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

static ma_device g_captureDevice;
static bool g_isCaptureDeviceInitialized = false;
// Callback này sẽ được Windows gọi liên tục mỗi khi có block âm thanh mới phát ra từ loa
void AudioCaptureCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    if (!pInput || frameCount == 0) return;

    // pInput chứa dữ liệu PCM thô dạng Float 32-bit từ hệ thống
    const float* rawSamples = (const float*)pInput;
    
    // Tổng số lượng mẫu số = số khung hình * số kênh (Ví dụ: frameCount * 2 kênh stereo)
    size_t totalSamples = frameCount * pDevice->capture.channels;

    // Bơm thẳng dữ liệu thô này vào hàng đợi an toàn đa luồng của bạn!
    AudioFilterManager::Instance().PushAudioSamples(rawSamples, totalSamples);
    
    (void)pOutput; // Không dùng đến đầu ra phát lại của callback này
}
// Khởi động luồng xử lý AI + Bật bộ capture âm thanh hệ thống
void AudioFilterManager::StartAudioAnalysis() {
    std::lock_guard<std::mutex> lock(m_analysisMutex);
    if (m_analysisThread.joinable()) return;

    m_stopAnalysis = false;

#ifdef _WIN32
    // 1. Khởi tạo Named Pipe độc bản, chế độ KHÔNG CHỜ (PIPE_NOWAIT)
    if (m_hAudioPipe == INVALID_HANDLE_VALUE) {
        m_hAudioPipe = CreateNamedPipe(
            TEXT("\\\\.\\pipe\\mpv_whisper_pipe"),
            PIPE_ACCESS_INBOUND,                   // C++ chỉ đọc dữ liệu vào
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, // Non-blocking
            1,                                     // Chỉ 1 instance duy nhất
            1024 * 512,                            // Bộ đệm Out
            1024 * 512,                            // Bộ đệm In
            0, NULL
        );
    }
    
    if (m_hAudioPipe == INVALID_HANDLE_VALUE) {
        AddLog("[AudioAnalysis] Failed to create Named Pipe.", LogLevel::Error);
        return;
    }
#endif

    // 2. Kích hoạt luồng phân tích AI ngầm đọc từ Pipe
    m_analysisThread = std::thread(&AudioFilterManager::AIAnalysisLoop, this);
    
    AddLog("[AudioAnalysis] Named Pipe setup completed. Waiting for MPV stream...", LogLevel::Info);
}

// Dừng luồng xử lý AI và Tắt bộ capture âm thanh
void AudioFilterManager::StopAudioAnalysis() {
    {
        std::lock_guard<std::mutex> lock(m_analysisMutex);
        if (!m_analysisThread.joinable()) return;
        m_stopAnalysis = true;
    }

    m_analysisCV.notify_one();
    m_analysisThread.join();

#ifdef _WIN32
    // Đóng và giải phóng kết nối Pipe
    if (m_hAudioPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hAudioPipe);
        m_hAudioPipe = INVALID_HANDLE_VALUE;
    }
#endif

    // Giải phóng hàng đợi cũ như cũ
    std::lock_guard<std::mutex> lock(m_analysisMutex);
    std::queue<std::vector<float>> emptyQueue;
    std::swap(m_audioDataQueue, emptyQueue);

    AddLog("[AudioAnalysis] Pipe closed and pipeline cleared.", LogLevel::Info);
}

// Nhận dữ liệu âm thanh thô từ luồng ngoài (ví dụ MPV hook) và nạp vào hàng đợi
void AudioFilterManager::PushAudioSamples(const float* samples, size_t sampleCount) {
    if (!samples || sampleCount == 0) return;
    if (m_stopAnalysis) return; // Không nhận dữ liệu nếu hệ thống đang tắt

    if (g_playbackStatus.isPaused) { 
        std::lock_guard<std::mutex> lock(m_subtitleMutex);
        m_currentSubtitle = ""; // Xóa chữ trên UI ngay lập tức khi bấm dừng video
        m_rollingSpeechBuffer.clear(); // Giải phóng bộ đệm giọng nói cũ
        m_samplesSinceLastInference = 0;
        return; 
    }

    // Sao chép dữ liệu thô vào một vector tạm
    std::vector<float> audioBuffer(samples, samples + sampleCount);

    {
        std::lock_guard<std::mutex> lock(m_analysisMutex);
        // Giới hạn hàng đợi tránh tràn bộ nhớ nếu AI xử lý quá chậm (Max 50 chunks ~ vài giây âm thanh)
        if (m_audioDataQueue.size() > 50) {
            m_audioDataQueue.pop(); // Bỏ bớt frame cũ nhất
        }
        m_audioDataQueue.push(std::move(audioBuffer));
    }

    // Thông báo cho luồng AI thức dậy xử lý chunk dữ liệu này
    m_analysisCV.notify_one();
}

// Vòng lặp xử lý ngầm (Chạy trên một luồng độc lập với UI và Player)
void AudioFilterManager::AIAnalysisLoop() {
    AddLog("[AudioAnalysis] Native Pipe Worker Loop active.", LogLevel::Info);

    const size_t READ_BUFFER_SIZE = 4096; // Tăng lên 4096 để đọc nhanh hơn, dọn dẹp Pipe sạch hơn
    std::vector<float> pipeReadBuffer(READ_BUFFER_SIZE);
    DWORD bytesRead = 0;

    while (true) {
        if (m_stopAnalysis) break;

#ifdef _WIN32
        if (m_hAudioPipe == INVALID_HANDLE_VALUE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Đọc dữ liệu từ Pipe liên tục để XẢ TRỐNG bộ đệm Pipe, tránh làm treo MPV
        BOOL success = ReadFile(
            m_hAudioPipe,
            pipeReadBuffer.data(),
            READ_BUFFER_SIZE * sizeof(float),
            &bytesRead,
            NULL
        );

        if (success && bytesRead > 0) {
            size_t samplesCaptured = bytesRead / sizeof(float);
            
            // CHỈ xử lý âm thanh nếu luồng phân tích đang thực sự cần (ví dụ video đang chạy và không bị pause)
            // Nếu app vừa mở, chưa bật tính năng AI sub hoặc video đang dừng, hàm Push này sẽ bỏ qua rất nhanh
            this->PushAudioSamples(pipeReadBuffer.data(), samplesCaptured);
        } else {
            // Lỗi hoặc Pipe trống dữ liệu (NOWAIT), nghỉ 5ms để nhường CPU cho luồng khác
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
    }
}
// Hàm hủy hoàn toàn thiết bị khi đóng ứng dụng
// Bạn có thể gọi hàm này trong Destructor của AudioFilterManager
void AudioFilterManager::CleanupAudioAnalysis() {
    if (g_isCaptureDeviceInitialized) {
        ma_device_uninit(&g_captureDevice);
        g_isCaptureDeviceInitialized = false;
    }
}
void AudioFilterManager::FlushAnalysisPipeline() {
    std::lock_guard<std::mutex> lock(m_analysisMutex);
    
    // 1. Xóa sạch hàng đợi âm thanh cũ để tránh AI dịch lại tiếng của video trước
    std::queue<std::vector<float>> emptyQueue;
    std::swap(m_audioDataQueue, emptyQueue);

    // 2. Xóa bộ đệm cuốn sliding window
    m_rollingSpeechBuffer.clear();
    m_samplesSinceLastInference = 0;

#ifdef _WIN32
    // 3. Ngắt kết nối instance cũ trên Pipe để sẵn sàng nhận luồng phát mới từ MPV
    if (m_hAudioPipe != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(m_hAudioPipe); 
    }
#endif
}