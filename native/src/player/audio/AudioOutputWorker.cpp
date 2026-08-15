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

bool AudioOutputWorker::Init(SpscRingBuffer<AudioBlock>* processedStream, PlayerStateSystem* stateSystem, AudioBackendType backend) {
    if (!processedStream) return false;

    m_processedStream = processedStream;
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
    if (m_isRunning) return;

    m_isRunning = true;
    m_workerThread = std::thread(&AudioOutputWorker::OutputLoop, this);
    GetThreadManager().Register(m_threadId, &m_workerThread);
}

void AudioOutputWorker::Stop() {
    if (!m_isRunning.exchange(false)) return;

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    GetThreadManager().Unregister(m_threadId);

    if (m_audioDevice) {
        m_audioDevice->Close();
    }

    m_processedStream = nullptr;
    m_stateSystem = nullptr;
}

void AudioOutputWorker::OutputLoop() {
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Audio, 
        std::cout << "[AudioOutputWorker] Resilient Output Loop started with Safe Pointer & Hardware Recovery.");

    std::vector<float> volumeAdjustedBuffer;
    uint32_t deviceErrorCount = 0;
    auto lastDeviceRetryTime = std::chrono::steady_clock::now();

    while (m_isRunning.load(std::memory_order_relaxed)) {
        try {
            if (!m_processedStream || !m_isRunning.load()) break;

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
                        m_audioDevice->Shutdown();
                    }

                    // Gọi đúng tên hàm CreateDeviceBackend
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

                if (const AudioBlock* dummy = m_processedStream->acquire_read()) {
                    m_processedStream->release_read();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // =========================================================================
            // 2. ĐỌC VÀ TỰ XỬ LÝ TRẠNG THÁI m_stateSystem
            // =========================================================================
            bool shouldSilence = false;
            bool isMuted = false;
            float currentVol = 1.0f;
            double currentPTS = 0.0;
            double audioDelay = 0.0;
            double playbackSpeed = 1.0;

            if (m_stateSystem) {

                m_stateSystem->ReadPlayback([&shouldSilence,&currentPTS,&playbackSpeed](const PlaybackModel& model) {
                    if(model.flags.isPaused || model.flags.isSeeking
                        || model.flags.eofReached || model.flags.isCoreIdle
                        || model.flags.isIdleActive)
                        shouldSilence = true;

                    currentPTS = model.timing.timePos;
                    playbackSpeed = (model.config.speed > 0.0) ? model.config.speed : 1.0;
                });
                m_stateSystem->ReadAudio([&isMuted,&currentVol,&audioDelay](const AudioModel& model) {
                    isMuted       = model.volume.isMuted;
                    currentVol    = static_cast<float>(model.volume.volume) / 100.0f;
                    audioDelay    = model.codec.audio_delay;
                });
            }

            if (shouldSilence) {
                if (m_audioDevice) {
                    m_audioDevice->FlushBuffers();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            // =========================================================================
            // 3. AN TOÀN TRUY CẬP RINGBUFFER & BLOCK VALIDATION
            // =========================================================================
            if (m_processedStream->occupancy() > 25) {
                for (int i = 0; i < 5; ++i) {
                    if (const AudioBlock* stale = m_processedStream->acquire_read()) {
                        m_processedStream->release_read();
                    }
                }
            }
            const AudioBlock* block = m_processedStream->acquire_read();

            if (!block) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (block->samples.empty() || block->sample_count() == 0) {
                m_processedStream->release_read();
                continue;
            }

            // --- KIỂM TRA GENERATION (SEEK SPAM PROTECTION) ---
            if (block->generation < m_lastGeneration) {
                m_processedStream->release_read();
                continue;
            }
            if (block->generation > m_lastGeneration) {
                m_lastGeneration = block->generation;
                
                if (m_audioDevice) {
                    m_audioDevice->FlushBuffers(); // Clear thiết bị phần cứng lập tức
                }
            }

            if (isMuted) {
                m_processedStream->release_read();
                continue;
            }

            // =========================================================================
            // 4. KIỂM TRA AV-SYNC / PTS DRIFT
            // =========================================================================
            if (currentPTS > 0.0 && block->pts > 0.0) {
                double targetPTS = currentPTS + audioDelay;
                double drift = std::abs(block->pts - targetPTS);

                // Nới lỏng ngưỡng drift nếu cần, 0.3s là khoảng an toàn chuẩn
                if (drift > 0.3) {
                    m_processedStream->release_read();
                    continue;
                }
            }

            // =========================================================================
            // 5. GHI ÂM THANH RA PHẦN CỨNG & AN TOÀN BỘ NHỚ
            // =========================================================================
            const size_t sampleCount = block->sample_count();
            const float* outputData = block->samples.data();

            if (std::abs(currentVol - 1.0f) > 0.001f) {
                volumeAdjustedBuffer.resize(sampleCount);
                for (size_t i = 0; i < sampleCount; ++i) {
                    volumeAdjustedBuffer[i] = block->samples[i] * currentVol;
                }
                outputData = volumeAdjustedBuffer.data();
            }

            bool writePerformed = false;
            if (m_audioDevice && m_audioDevice->IsReady()) {
                const uint32_t baseMaxQueuedBytes = static_cast<uint32_t>(48000 * 2 * sizeof(float) * 0.025f);
                const uint32_t adjustedMaxQueuedBytes = static_cast<uint32_t>(baseMaxQueuedBytes * playbackSpeed);

                while (m_isRunning && m_audioDevice->GetQueuedSizeBytes() > adjustedMaxQueuedBytes) {
                    if (m_processedStream->peek_generation() > m_lastGeneration) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                if (m_isRunning) {
                    // Tùy theo return type của Write (void hay bool)
                    m_audioDevice->Write(outputData, sampleCount);
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