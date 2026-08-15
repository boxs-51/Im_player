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
                std::cout << "[AudioOutputWorker] WASAPI backend not yet implemented, fallback to SDL2.");
            return std::make_unique<SdlAudioDevice>();
            
        default:
            LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Audio, 
                std::cout << "[AudioOutputWorker] Unknown backend type, fallback to SDL2.");
            return std::make_unique<SdlAudioDevice>();
    }
}

bool AudioOutputWorker::Init(SpscRingBuffer<AudioBlock>* processedStream, AudioBackendType backend) {
    if (!processedStream) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
            std::cout << "[AudioOutputWorker] Init failed: processedStream pointer is null.");
        return false;
    }

    m_processedStream = processedStream;
    m_threadId = "AudioOutputWorker_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    m_currentBackendType = backend;
    m_audioDevice = CreateDeviceBackend(m_currentBackendType);

    if (!m_audioDevice || !m_audioDevice->Open(48000, 2)) {
        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
            std::cout << "[AudioOutputWorker] Failed to open Audio Device Backend.");
        return false;
    }

    return true;
}

bool AudioOutputWorker::SwitchBackend(AudioBackendType newBackend) {
    bool wasRunning = m_isRunning.load(std::memory_order_relaxed);
    
    if (wasRunning) {
        Stop();
    }

    m_currentBackendType = newBackend;
    m_audioDevice = CreateDeviceBackend(m_currentBackendType);
    bool success = m_audioDevice && m_audioDevice->Open(48000, 2);

    if (wasRunning && success) {
        Start();
    }
    return success;
}

void AudioOutputWorker::Start() {
    if (m_isRunning.load(std::memory_order_relaxed)) return;

    m_isRunning.store(true, std::memory_order_release);
    m_workerThread = std::thread(&AudioOutputWorker::OutputLoop, this);
    GetThreadManager().Register(m_threadId, &m_workerThread);
}

void AudioOutputWorker::Stop() {
    if (!m_isRunning.exchange(false, std::memory_order_acq_rel)) return;

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    GetThreadManager().Unregister(m_threadId);

    if (m_audioDevice) {
        m_audioDevice->Close();
    }

    m_processedStream = nullptr;
}

void AudioOutputWorker::OutputLoop() {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
        std::cout << "[AudioOutputWorker] Resilient Output Loop started with Safe Single-Consumer Pattern.");

    std::vector<float> volumeAdjustedBuffer;
    uint32_t deviceErrorCount = 0;
    auto lastDeviceRetryTime = std::chrono::steady_clock::now();

    while (m_isRunning.load(std::memory_order_relaxed)) {
        try {
            if (!m_processedStream || !m_isRunning.load(std::memory_order_relaxed)) break;

            // =========================================================================
            // 1. TỰ KHÔI PHỤC THIẾT BỊ PHẦN CỨNG (HARDWARE RECOVERY / AUTO-REINIT)
            // =========================================================================
            if (!m_audioDevice || !m_audioDevice->IsReady()) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDeviceRetryTime).count() > 1000) {
                    lastDeviceRetryTime = now;
                    LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Audio, 
                        std::cout << "[AudioOutputWorker] Audio device unavailable. Attempting auto-recovery...");

                    if (m_audioDevice) {
                        m_audioDevice->Close();
                    }

                    m_audioDevice = CreateDeviceBackend(m_currentBackendType);
                    if (m_audioDevice && m_audioDevice->Open(48000, 2)) {
                        LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
                            std::cout << "[AudioOutputWorker] Audio device auto-recovery SUCCESSFUL!");
                        deviceErrorCount = 0;
                    } else {
                        LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
                            std::cout << "[AudioOutputWorker] Audio device auto-recovery failed. Will retry...");
                    }
                }

                // Xóa bớt block tồn đọng duy nhất từ luồng OutputWorker khi mất thiết bị
                if (const AudioBlock* dummy = m_processedStream->acquire_read()) {
                    m_processedStream->release_read();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // =========================================================================
            // 3. TRUY CẤP RINGBUFFER DÀNH RIÊNG CHO CONSUMER & XỬ LÝ GENERATION (SEEK)
            // =========================================================================
            const AudioBlock* block = m_processedStream->acquire_read();

            if (!block) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (block->samples.empty() || block->sample_count() == 0) {
                m_processedStream->release_read();
                continue;
            }

            // --- KIỂM TRA GENERATION KHI SEEK (SINGLE-CONSUMER DRAIN) ---
            if (block->generation < m_lastGeneration) {
                // Bỏ qua block lỗi thời tạo ra trước thời điểm Seek
                m_processedStream->release_read();
                continue;
            }
            if (block->generation > m_lastGeneration) {
                // Phát hiện Seek mới -> Cập nhật Generation và xả sạch phần cứng âm thanh ngay lập tức
                m_lastGeneration = block->generation;
                
                if (m_audioDevice) {
                    m_audioDevice->FlushBuffers();
                }
            }

            bool writePerformed = false;
            if (m_audioDevice && m_audioDevice->IsReady()) {

                //while (m_isRunning.load(std::memory_order_relaxed)) {
                //    if (m_processedStream->peek_generation() > m_lastGeneration) {
                //        break;
                //    }
                //    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                //}

                if (m_isRunning.load(std::memory_order_relaxed)) {
                    m_audioDevice->Write(block->samples.data(), block->sample_count());
                    writePerformed = true;
                }
            }

            if (!writePerformed) {
                deviceErrorCount++;
                if (deviceErrorCount > 10) {
                    LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
                        std::cout << "[AudioOutputWorker] Hardware write failed repeatedly. Marking device as offline.");
                    if (m_audioDevice) {
                        m_audioDevice->SetReady(false);
                    }
                }
            } else {
                deviceErrorCount = 0;
            }

            m_processedStream->release_read();

        } catch (const std::exception& e) {
            LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
                std::cout << "[AudioOutputWorker] Exception caught in loop: " << e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } catch (...) {
            LOG_NO_KEY(1, LogLevel::Error, LogCategory::Audio, 
                std::cout << "[AudioOutputWorker] Unknown crash prevented in loop.");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, std::cout << "[AudioOutputWorker] Output thread safely stopped.");
}