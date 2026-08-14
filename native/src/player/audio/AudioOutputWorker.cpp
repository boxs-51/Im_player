#include "AudioOutputWorker.h"
#include "SdlAudioDevice.h"
#include "log.h"
#include "threads/thread_manager.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

AudioOutputWorker::AudioOutputWorker()
    : m_threadId("") {}

AudioOutputWorker::~AudioOutputWorker() {
    Stop();
}

std::unique_ptr<IAudioOutputDevice> AudioOutputWorker::CreateDeviceBackend(AudioBackendType type) {
    switch (type) {
        case AudioBackendType::SDL2:
            return std::make_unique<SdlAudioDevice>();
        case AudioBackendType::WASAPI:
            LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Audio, 
                std::cout << "[AudioOutputWorker] WASAPI not implemented, fallback to SDL2");
            return std::make_unique<SdlAudioDevice>();
        default:
            return std::make_unique<SdlAudioDevice>();
    }
}

bool AudioOutputWorker::Init(AudioCaptureManager* captureManager, PlayerStateSystem* stateSystem, AudioBackendType backend) {
    if (!captureManager) return false;

    m_captureManager = captureManager;
    m_stateSystem = stateSystem;
    m_threadId = "AudioOutputWorker_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    m_currentBackendType = backend;
    m_audioDevice = CreateDeviceBackend(m_currentBackendType);

    if (!m_audioDevice || !m_audioDevice->Open(48000, 2)) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, std::cout << "[AudioOutputWorker] Failed to open Audio Device Backend.");
        return false;
    }

    return true;
}

bool AudioOutputWorker::SwitchBackend(AudioBackendType newBackend) {
    bool wasRunning = m_isRunning.load();
    
    // 1. Dừng Worker Thread & Close Device cũ hoàn toàn trước
    if (wasRunning) {
        Stop();
    }

    // 2. Tạo & Mở Device Backend mới
    m_currentBackendType = newBackend;
    m_audioDevice = CreateDeviceBackend(m_currentBackendType);
    bool success = m_audioDevice && m_audioDevice->Open(48000, 2);

    // 3. Khởi động lại thread nếu trước đó đang chạy
    if (wasRunning && success) {
        Start();
    }
    return success;
}

void AudioOutputWorker::Start() {
    if (m_isRunning) return;

    m_isRunning = true;
    m_workerThread = std::thread(&AudioOutputWorker::OutputLoop, this);
    GetThreadManager().Register(m_threadId, &m_workerThread);
}

void AudioOutputWorker::Stop() {
    if (!m_isRunning.exchange(false)) return;

    // 1. Chờ worker thread kết thúc HOÀN TOÀN trước tiên!
    // Tránh việc OutputLoop tiếp tục truy cập m_audioDevice khi đang Close/Delete.
    if (m_workerThread.joinable()) {
        m_workerThread.join();   // Chờ Thread thoát sạch sẽ
    }

    // 2. Unregister khỏi ThreadManager SAU KHI thread đã join
    GetThreadManager().Unregister(m_threadId);

    // 3. Đóng phần cứng an toàn
    if (m_audioDevice) {
        m_audioDevice->Close();
    }

    // 4. Reset con trỏ tham chiếu
    m_captureManager = nullptr;
    m_stateSystem = nullptr;
}

void AudioOutputWorker::OutputLoop() {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioOutputWorker] Output thread started.");

    AudioBlock block;
    std::vector<float> volumeAdjustedBuffer;

    while (m_isRunning.load(std::memory_order_relaxed)) {
        try {
 
        auto* captureMgr = m_captureManager;
            if (!captureMgr || !m_isRunning.load()) break;

        // Pop AudioBlock từ RingBuffer (Lock-free)
        if (m_captureManager->GetRawStream().try_pop(block)) {

            // --- TỰ ĐỘNG FLUSH KHI SEEK / ĐỔI TRACK ---
            if (block.generation < m_lastGeneration) {
                continue; // Bỏ qua dữ liệu cũ từ generation trước
            }
            if (block.generation > m_lastGeneration) {
                m_lastGeneration = block.generation;
                if (m_audioDevice) {
                    m_audioDevice->FlushBuffers();
                }
            }

            // --- TỰ ĐỘNG ĐỌC VOLUME / MUTE TỪ PlayerStateSystem ---
            float currentVol = 1.0f;
            bool isMuted = false;

            if (m_stateSystem) {
                AudioModel audio = m_stateSystem->GetAudioModel();
                currentVol = static_cast<float>(audio.volume.volume) / 100.0f;
                isMuted = audio.volume.isMuted;
            }

            if (isMuted) {
                // Khi Mute, ngủ ngắn 2ms để giải phóng CPU hoàn toàn
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            const size_t sampleCount = block.sample_count();
            const float* outputData = block.samples.data();

            // Áp dụng Volume Scaling
            if (std::abs(currentVol - 1.0f) > 0.001f) {
                volumeAdjustedBuffer.resize(sampleCount);
                for (size_t i = 0; i < sampleCount; ++i) {
                    volumeAdjustedBuffer[i] = block.samples[i] * currentVol;
                }
                outputData = volumeAdjustedBuffer.data();
            }

            // --- ĐẨY DATA SANG MẠNG PHẦN CỨNG ---
            if (m_audioDevice) {
                // Tối ưu Latency: Chỉ đệm tối đa ~20ms audio trong hardware queue
                constexpr uint32_t maxQueuedBytes = static_cast<uint32_t>(48000 * 2 * sizeof(float) * 0.02f);
                
                // GIẢI PHÁP TỐI ƯU CPU: Thay yield() bằng sleep_for(1ms) khi Hardware Queue bị đầy
                while (m_isRunning && m_audioDevice->GetQueuedSizeBytes() > maxQueuedBytes) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                if (m_isRunning) {
                    m_audioDevice->Write(outputData, sampleCount);
                }
            }
        } else {
            // GIẢI PHÁP TỐI ƯU CPU: Ngủ 1ms khi RingBuffer rỗng thay vì yield() liên tục
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        }catch (...) {
            break;
        }
    }

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioOutputWorker] Output thread finished.");
}