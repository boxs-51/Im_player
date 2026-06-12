#include "audio_filter_manager.h"
#include <iostream>

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
    
    // 1. Khởi chạy luồng xử lý AI ngầm trước
    m_analysisThread = std::thread(&AudioFilterManager::AIAnalysisLoop, this);

    // 2. Cấu hình thiết bị Thu âm Loopback từ Hệ thống
    if (!g_isCaptureDeviceInitialized) {
        // SỬA DÒNG NÀY: Đổi sang ma_device_type_loopback để lấy âm thanh từ Loa/Hệ thống
        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_loopback);
        
        // Cấu hình định dạng chuẩn cho AI
        deviceConfig.capture.format      = ma_format_f32; 
        deviceConfig.capture.channels    = 1;              // Ép về 1 kênh (Mono) để AI xử lý cho nhẹ
        deviceConfig.sampleRate          = 16000;          // Chuẩn tần số của các mô hình AI nhận diện giọng nói
        deviceConfig.dataCallback        = AudioCaptureCallback;

        // Dòng subType này giữ nguyên comment (không cần thiết nữa vì đã dùng ma_device_type_loopback ở trên)
        // deviceConfig.wasapi.subType   = ma_wasapi_sub_type_loopback; 

        if (ma_device_init(NULL, &deviceConfig, &g_captureDevice) != MA_SUCCESS) {
            AddLog("[AudioAnalysis] Failed to initialize WASAPI Loopback device.", LogLevel::Error);
            return;
        }
        g_isCaptureDeviceInitialized = true;
    }

    // Kích hoạt thiết bị bắt đầu thu mẫu
    ma_device_start(&g_captureDevice);
    AddLog("[AudioAnalysis] System and Loopback Capture started at 16kHz Mono.", LogLevel::Info);
}

// Dừng luồng xử lý AI và Tắt bộ capture âm thanh
void AudioFilterManager::StopAudioAnalysis() {
    {
        std::lock_guard<std::mutex> lock(m_analysisMutex);
        if (!m_analysisThread.joinable()) return;
        m_stopAnalysis = true;
    }

    // Tắt thiết bị thu âm trước để ngừng đẩy dữ liệu vào hàng đợi
    if (g_isCaptureDeviceInitialized) {
        ma_device_stop(&g_captureDevice);
    }
    
    m_analysisCV.notify_one();
    m_analysisThread.join();
    
    // Giải phóng bộ nhớ hàng đợi mẫu âm thanh cũ
    std::lock_guard<std::mutex> lock(m_analysisMutex);
    std::queue<std::vector<float>> emptyQueue;
    std::swap(m_audioDataQueue, emptyQueue);

    AddLog("[AudioAnalysis] Capture stopped and pipeline cleared.", LogLevel::Info);
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
    AddLog("[AudioAnalysis] Native Worker Loop with Sliding Window active.", LogLevel::Info);

    // Định nghĩa các tham số cấu hình cho 16000Hz (Mono)
    const size_t SAMPLE_RATE = 16000;
    const size_t STRIDE_SIZE = SAMPLE_RATE * 0.5;       // 500ms = 8000 samples (Bước nhảy gọi AI)
    const size_t TOTAL_WINDOW_SIZE = SAMPLE_RATE * 2.0; // 2.0 giây = 32000 samples (Độ dài ngữ cảnh gửi AI)

    while (true) {
        std::vector<float> currentChunk;

        {
            std::unique_lock<std::mutex> lock(m_analysisMutex);
            m_analysisCV.wait(lock, [this] { 
                return !m_audioDataQueue.empty() || m_stopAnalysis; 
            });

            if (m_stopAnalysis && m_audioDataQueue.empty()) {
                break;
            }

            currentChunk = std::move(m_audioDataQueue.front());
            m_audioDataQueue.pop();
        }

        if (!currentChunk.empty()) {
            size_t size = currentChunk.size();
            m_totalSamplesCaptured += size;

            // 1. TÍNH TOÁN RMS & PEAK (Code gốc của bạn)
            double sumOfSquares = 0.0;
            float maxPeak = 0.0f;
            size_t zeroCrossings = 0; // Thêm bộ đếm đổi dấu để phân biệt nhạc/nói

            for (size_t i = 0; i < size; ++i) {
                float sample = currentChunk[i];
                sumOfSquares += (sample * sample);
                
                float absSample = std::fabs(sample);
                if (absSample > maxPeak) {
                    maxPeak = absSample;
                }

                // Tính Zero-Crossing Rate (ZCR) phục vụ phân loại thô
                if (i > 0 && ((currentChunk[i] >= 0 && currentChunk[i - 1] < 0) || (currentChunk[i] < 0 && currentChunk[i - 1] >= 0))) {
                    zeroCrossings++;
                }
            }

            float rms = (size > 0) ? std::sqrt(static_cast<float>(sumOfSquares / size)) : 0.0f;
            float zcr = (size > 0) ? static_cast<float>(zeroCrossings) / size : 0.0f;

            m_analysisRMS.store(rms);
            m_analysisPeak.store(maxPeak);

            // Cập nhật lịch sử đồ thị ImGui
            {
                std::lock_guard<std::mutex> lock(m_historyMutex);
                if (!m_rmsHistory.empty()) {
                    m_rmsHistory.erase(m_rmsHistory.begin());
                    m_rmsHistory.push_back(rms);
                }
            }

            // Lấy ngữ cảnh hiện tại từ hệ thống Player
            AudioContext ctx = this->ExtractCurrentContext(); 

            // 1. Nếu đang pause thì bỏ qua, giải phóng nhanh luồng
            if (ctx.is_paused) {
                std::lock_guard<std::mutex> lock(m_subtitleMutex);
                m_currentSubtitle = "";
                m_rollingSpeechBuffer.clear();
                continue;
            }

            // 2. BỘ LỌC PHÂN LOẠI "NGU" (Dumb Classifier)
            // Nếu RMS quá nhỏ (< 0.002): Coi như yên lặng. 
            // Nếu ZCR quá cao (> 0.32) liên tục trong khi nhạc đang phát kịch khung: Khả năng cao là nhạc nền/tiếng xì.
            bool isSpeech = true;
            if (rms < 0.002f) {
                isSpeech = false;
                // Khi yên lặng kéo dài, xóa bớt đệm cũ để tránh câu sau bị dính chữ của câu trước
                if (m_rollingSpeechBuffer.size() > 0 && m_samplesSinceLastInference == 0) {
                    m_rollingSpeechBuffer.clear();
                }
            } // Nếu dữ liệu ngoại báo đây là file CHỈ CÓ AUDIO (Nghe nhạc mp3/flac)
            else if (ctx.is_audio_only) {
                // Siết chặt điều kiện: Nhạc thường có dải tần rộng và ZCR biến động liên tục
                // Nếu ZCR > 0.25 trong môi trường thuần nhạc -> Khả năng cao là tiếng nhạc cụ/hi-hat, không phải tiếng người nói
                if (zcr > 0.25f) {
                    isSpeech = false; 
                }
            }
            // Nếu là Phim (Có hình có tiếng)
            else {
                // Phim thông thường: Lọc theo ngưỡng tiêu chuẩn
                if (zcr > 0.32f) {
                    isSpeech = false;
                }
            }
            // 3. CƠ CHẾ CỬA SỔ TRƯỢT (Sliding Window)
            if (isSpeech) {
                // Nạp chunk hiện tại vào bộ đệm cuốn
                m_rollingSpeechBuffer.insert(m_rollingSpeechBuffer.end(), currentChunk.begin(), currentChunk.end());
                m_samplesSinceLastInference += size;

                // Giới hạn bộ đệm cuốn tối đa 3 giây để tránh phình bộ nhớ
                if (m_rollingSpeechBuffer.size() > SAMPLE_RATE * 3) {
                    size_t eraseSize = m_rollingSpeechBuffer.size() - (SAMPLE_RATE * 3);
                    m_rollingSpeechBuffer.erase(m_rollingSpeechBuffer.begin(), m_rollingSpeechBuffer.begin() + eraseSize);
                }

                // CỨ ĐỦ 500MS (STRIDE_SIZE) -> TRÍCH XUẤT ĐỂ CHUẨN BỊ GỬI AI
                if (m_samplesSinceLastInference >= STRIDE_SIZE) {
                    
                    // Trích xuất block dữ liệu dài tối đa 2 giây (TOTAL_WINDOW_SIZE) về phía cuối đệm
                    std::vector<float> whisperInput;
                    if (m_rollingSpeechBuffer.size() <= TOTAL_WINDOW_SIZE) {
                        whisperInput = m_rollingSpeechBuffer;
                    } else {
                        whisperInput.assign(m_rollingSpeechBuffer.end() - TOTAL_WINDOW_SIZE, m_rollingSpeechBuffer.end());
                    }

                    // ------------------------------------------------------------------
                    // KHU VỰC THỬ NGHIỆM (MOCK TEST WHISPER)
                    // ------------------------------------------------------------------
                    // Ở bước này, ta giả lập việc Whisper trả về text để xem log nhảy chuẩn chưa
                    std::string mockText = "[Test Sub] Đang phân tích " + std::to_string(whisperInput.size()) + " mẫu...";
                    
                    {
                        std::lock_guard<std::mutex> lock(m_subtitleMutex);
                        m_currentSubtitle = mockText;
                    }
                    
                    // Bạn có thể in ra console để theo dõi nhịp độ nhảy chữ mỗi 500ms
                    // std::cout << "[AI Log] Trigger Whisper Inference. Buffer Size: " << whisperInput.size() << std::endl;

                    m_samplesSinceLastInference = 0; // Reset bộ đếm bước đi
                }
            }
        }
    }

    AddLog("[AudioAnalysis] Native Worker Loop has terminated.", LogLevel::Info);
}
// Hàm hủy hoàn toàn thiết bị khi đóng ứng dụng
// Bạn có thể gọi hàm này trong Destructor của AudioFilterManager
void AudioFilterManager::CleanupAudioAnalysis() {
    if (g_isCaptureDeviceInitialized) {
        ma_device_uninit(&g_captureDevice);
        g_isCaptureDeviceInitialized = false;
    }
}