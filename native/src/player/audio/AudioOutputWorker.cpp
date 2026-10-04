#include "AudioOutputWorker.h"
#include "SdlAudioDevice.h"
#include "log.h"
#include "threads/thread_manager.h"
#include "common/LifecycleEvidence.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

AudioOutputWorker::AudioOutputWorker()
{
    LifecycleEvidence::Emit(
        "AudioOutputWorker",
        "CREATE",
        LifecycleEvidence::PointerIdentity(this));
}

AudioOutputWorker::~AudioOutputWorker() {
    Stop();
    LifecycleEvidence::Emit(
        "AudioOutputWorker",
        "DESTROY",
        LifecycleEvidence::PointerIdentity(this));
}

std::unique_ptr<IAudioOutputDevice> AudioOutputWorker::CreateDeviceBackend(AudioBackendType type) {
    switch (type) {
        case AudioBackendType::SDL2:
            return std::make_unique<SdlAudioDevice>();
            
        case AudioBackendType::WASAPI:
            LOG(1, LogLevel::Warning, LogCategory::Audio, 
                "[AudioOutputWorker] WASAPI backend not yet implemented, fallback to SDL2.");
            return std::make_unique<SdlAudioDevice>();
            
        default:
            LOG(1, LogLevel::Warning, LogCategory::Audio, 
                "[AudioOutputWorker] Unknown backend type, fallback to SDL2.");
            return std::make_unique<SdlAudioDevice>();
    }
}

bool AudioOutputWorker::Init(SpscConsumer<AudioBlock> processedStream, AudioBackendType backend) {

    //if (!processedStream) {
    //    LOG(1, LogLevel::Error, LogCategory::Audio, 
    //        std::cout << "[AudioOutputWorker] Init failed: processedStream pointer is null.");
    //    return false;
    //}

    m_processedStream.emplace(std::move(processedStream));
    m_threadId = "AudioOutputWorker_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    m_currentBackendType = backend;
    m_audioDevice = CreateDeviceBackend(m_currentBackendType);

    if (!m_audioDevice || !m_audioDevice->Open(kCanonicalAudioSampleRate, kCanonicalAudioChannels)) {
        LOG(1, LogLevel::Error, LogCategory::Audio, 
            "[AudioOutputWorker] Failed to open Audio Device Backend.");
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
    bool success = m_audioDevice && m_audioDevice->Open(kCanonicalAudioSampleRate, kCanonicalAudioChannels);

    if (wasRunning && success) {
        Start();
    }
    return success;
}

AudioOutputMetrics AudioOutputWorker::GetMetrics() const {
    AudioOutputMetrics snapshot;
    snapshot.blocksWritten = m_metrics.blocksWritten.load(std::memory_order_relaxed);
    snapshot.blocksDroppedFormatMismatch =
        m_metrics.blocksDroppedFormatMismatch.load(std::memory_order_relaxed);
    snapshot.queueUnderflowEvents =
        m_metrics.queueUnderflowEvents.load(std::memory_order_relaxed);
    snapshot.writeFailures = m_metrics.writeFailures.load(std::memory_order_relaxed);
    snapshot.queuedBytes = m_metrics.queuedBytes.load(std::memory_order_relaxed);
    snapshot.queueHighWaterBytes =
        m_metrics.queueHighWaterBytes.load(std::memory_order_relaxed);
    snapshot.queuedMilliseconds =
        m_metrics.queuedMilliseconds.load(std::memory_order_relaxed);
    snapshot.sampleRate = kCanonicalAudioSampleRate;
    snapshot.channels = kCanonicalAudioChannels;
    return snapshot;
}

void AudioOutputWorker::Start() {
    if (m_isRunning.load(std::memory_order_relaxed)) return;

    m_isRunning.store(true, std::memory_order_release);
    m_workerThread = std::thread(&AudioOutputWorker::OutputLoop, this);
    GetThreadManager().Register(m_threadId, &m_workerThread);
    LifecycleEvidence::Emit("AudioOutputWorker", "START", LifecycleEvidence::PointerIdentity(this));
}

void AudioOutputWorker::Stop() {
    if (!m_isRunning.exchange(false, std::memory_order_acq_rel)) return;

    LifecycleEvidence::Emit("AudioOutputWorker", "STOP", LifecycleEvidence::PointerIdentity(this));
    if (m_workerThread.joinable()) {
        m_workerThread.join();
        LifecycleEvidence::Emit("AudioOutputWorker", "JOIN", LifecycleEvidence::PointerIdentity(this));
    }

    GetThreadManager().Unregister(m_threadId);

    if (m_audioDevice) {
        m_audioDevice->Close();
    }
}

void AudioOutputWorker::OutputLoop() {
    LOG(1, LogLevel::Info, LogCategory::Audio, 
        "[AudioOutputWorker] Resilient Output Loop started with Safe Single-Consumer Pattern.");

    std::vector<float> volumeAdjustedBuffer;
    uint32_t deviceErrorCount = 0;
    auto lastDeviceRetryTime = std::chrono::steady_clock::now();
    bool hasQueuedAudio = false;
    bool queueEmptyLatched = false;

    const auto updateQueueMetrics = [this](uint32_t queuedBytes) {
        m_metrics.queuedBytes.store(queuedBytes, std::memory_order_relaxed);

        const double bytesPerSecond =
            static_cast<double>(kCanonicalAudioSampleRate) *
            static_cast<double>(kCanonicalAudioChannels) *
            static_cast<double>(sizeof(float));
        const double queuedMs =
            (bytesPerSecond > 0.0)
                ? (1000.0 * static_cast<double>(queuedBytes) / bytesPerSecond)
                : 0.0;
        m_metrics.queuedMilliseconds.store(queuedMs, std::memory_order_relaxed);

        uint32_t previousHigh =
            m_metrics.queueHighWaterBytes.load(std::memory_order_relaxed);
        while (queuedBytes > previousHigh &&
               !m_metrics.queueHighWaterBytes.compare_exchange_weak(
                   previousHigh,
                   queuedBytes,
                   std::memory_order_relaxed,
                   std::memory_order_relaxed)) {
        }
    };

    while (m_isRunning.load(std::memory_order_relaxed)) {
        try {
            if (/*!m_processedStream ||*/ !m_isRunning.load(std::memory_order_relaxed)) break;

            // =========================================================================
            // 1. TỰ KHÔI PHỤC THIẾT BỊ PHẦN CỨNG (HARDWARE RECOVERY / AUTO-REINIT)
            // =========================================================================
            if (!m_audioDevice || !m_audioDevice->IsReady()) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDeviceRetryTime).count() > 1000) {
                    lastDeviceRetryTime = now;
                    LOG(1, LogLevel::Warning, LogCategory::Audio, 
                        "[AudioOutputWorker] Audio device unavailable. Attempting auto-recovery...");

                    if (m_audioDevice) {
                        m_audioDevice->Close();
                    }

                    m_audioDevice = CreateDeviceBackend(m_currentBackendType);
                    if (m_audioDevice && m_audioDevice->Open(kCanonicalAudioSampleRate, kCanonicalAudioChannels)) {
                        LOG(1, LogLevel::Info, LogCategory::Audio, 
                            "[AudioOutputWorker] Audio device auto-recovery SUCCESSFUL!");
                        deviceErrorCount = 0;
                    } else {
                        LOG(1, LogLevel::Error, LogCategory::Audio, 
                            "[AudioOutputWorker] Audio device auto-recovery failed. Will retry...");
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
                if (m_audioDevice && m_audioDevice->IsReady()) {
                    const uint32_t queuedBytes = m_audioDevice->GetQueuedSizeBytes();
                    updateQueueMetrics(queuedBytes);
                    if (hasQueuedAudio && queuedBytes == 0 && !queueEmptyLatched) {
                        m_metrics.queueUnderflowEvents.fetch_add(1, std::memory_order_relaxed);
                        queueEmptyLatched = true;
                    } else if (queuedBytes > 0) {
                        queueEmptyLatched = false;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (block->samples.empty() || block->sample_count() == 0) {
                m_processedStream->release_read();
                continue;
            }

            if (block->format.sampleRate != kCanonicalAudioSampleRate ||
                block->format.channels != kCanonicalAudioChannels ||
                block->format.format != AudioSampleFormat::Float32) {
                m_metrics.blocksDroppedFormatMismatch.fetch_add(1, std::memory_order_relaxed);
                LOG(1, LogLevel::Error, LogCategory::Audio,
                    "[AudioOutputWorker] Dropping block with non-canonical format: rate=%u channels=%u format=%u",
                    block->format.sampleRate,
                    static_cast<unsigned int>(block->format.channels),
                    static_cast<unsigned int>(block->format.format));
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
                    writePerformed = m_audioDevice->IsReady();
                    if (writePerformed) {
                        hasQueuedAudio = true;
                        queueEmptyLatched = false;
                        m_metrics.blocksWritten.fetch_add(1, std::memory_order_relaxed);
                        updateQueueMetrics(m_audioDevice->GetQueuedSizeBytes());
                    }
                }
            }

            if (!writePerformed) {
                m_metrics.writeFailures.fetch_add(1, std::memory_order_relaxed);
                deviceErrorCount++;
                if (deviceErrorCount > 10) {
                    LOG(1, LogLevel::Error, LogCategory::Audio, 
                        "[AudioOutputWorker] Hardware write failed repeatedly. Marking device as offline.");
                    if (m_audioDevice) {
                        m_audioDevice->SetReady(false);
                    }
                }
            } else {
                deviceErrorCount = 0;
            }

            m_processedStream->release_read();

        } catch (const std::exception& e) {
            LOG(1, LogLevel::Error, LogCategory::Audio, 
                "[AudioOutputWorker] Exception caught in loop: %s", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } catch (...) {
            LOG(1, LogLevel::Error, LogCategory::Audio, 
                "[AudioOutputWorker] Unknown crash prevented in loop.");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    LOG(1, LogLevel::Info, LogCategory::Audio, "[AudioOutputWorker] Output thread safely stopped.");
}