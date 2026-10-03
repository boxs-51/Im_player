#include "AudioCaptureManager.h"
#include "PlayerStateSystem.h"
#include "log.h"
#include "threads/thread_manager.h"
#include "common/LifecycleEvidence.h"
#include <iostream>
#include <algorithm>

AudioCaptureManager::AudioCaptureManager()
{
    LifecycleEvidence::Emit(
        "AudioCaptureManager",
        "CREATE",
        LifecycleEvidence::PointerIdentity(this));
}

AudioCaptureManager::~AudioCaptureManager() {
    Shutdown();
    LifecycleEvidence::Emit(
        "AudioCaptureManager",
        "DESTROY",
        LifecycleEvidence::PointerIdentity(this));
}

std::string AudioCaptureManager::GetPipeName() const { 
    return m_pipeName; 
}


void AudioCaptureManager::UpdateFormatCacheFromState() {
    if (!m_stateSystem) return;

    // Khởi tạo mặc định hợp lệ tránh giá trị rác
    int sampleRate = 48000;
    int channel_count = 2;

    m_stateSystem->ReadAudio([&sampleRate, &channel_count](const AudioModel& model) {
        if (model.params.asamplerate > 0)
            sampleRate = model.params.asamplerate;
        if (model.params.channel_count > 0)
            channel_count = model.params.channel_count;
    });

    // Clamp giá trị đảm bảo nằm trong khoảng an toàn cho stereo pipeline
    sampleRate = std::clamp(sampleRate, 8000, 192000);
    channel_count = std::clamp(channel_count, 1, static_cast<int>(kMaxAudioChannels));

    m_formatCache.sampleRate = static_cast<uint32_t>(sampleRate);
    m_formatCache.channels = static_cast<uint8_t>(channel_count);
}

AudioPipelineMetrics AudioCaptureManager::GetMetrics() const {
    AudioPipelineMetrics snapshot;
    snapshot.blocksReceived = m_metrics.blocksReceived.load(std::memory_order_relaxed);
    snapshot.blocksDropped = m_metrics.blocksDropped.load(std::memory_order_relaxed);
    snapshot.bytesReceived = m_metrics.bytesReceived.load(std::memory_order_relaxed);
    snapshot.ringOverflows = m_metrics.ringOverflows.load(std::memory_order_relaxed);
    snapshot.lastSequence = m_metrics.lastSequence.load(std::memory_order_relaxed);
    snapshot.lastPTS = m_metrics.lastPTS.load(std::memory_order_relaxed);
    return snapshot;
}

bool AudioCaptureManager::Init(mpv_handle* mpv, PlayerStateSystem* stateSystem, SpscProducer<AudioBlock> producer) {
    if (!mpv || !stateSystem) return false;
    LOG(1, LogLevel::Info, LogCategory::Audio, "[AudioCaptureManager] Initializing...");
    
    m_producer.emplace(std::move(producer));
    m_mpv = mpv;
    m_stateSystem = stateSystem;
    m_threadId = "AudioCapture_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    m_pipeName = "\\\\.\\pipe\\mpv_pcm_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    UpdateFormatCacheFromState();

    StartCapture();

    mpv_set_property_string(m_mpv, "ao", "pcm");
    mpv_set_property_string(m_mpv, "ao-pcm-file", m_pipeName.c_str());

    return true;
}

void AudioCaptureManager::Shutdown() {
    StopCapture();
    LOG(1, LogLevel::Info, LogCategory::Audio, "[AudioCaptureManager] Shutdown complete.");
}

void AudioCaptureManager::StartCapture() {
    if (m_isRunning) {
        m_isCapturing = true;
        return;
    }

    m_isRunning = true;
    m_isCapturing = true;
    m_sequence = 0;
    m_currentPts = 0.0;
    m_activeGeneration = 0;
    m_wasSeeking = false;

    m_captureThread = std::thread(&AudioCaptureManager::CaptureLoop, this);
    GetThreadManager().Register(m_threadId, &m_captureThread);
    LifecycleEvidence::Emit("AudioCaptureManager", "START", m_threadId.ToString());
}

void AudioCaptureManager::StopCapture() {
    if (!m_isRunning.exchange(false)) {
        return;
    }

    LifecycleEvidence::Emit("AudioCaptureManager", "STOP", m_threadId.ToString());
    m_isCapturing = false;

    HANDLE hPipe = m_atomicPipeHandle.load();
    if (hPipe != INVALID_HANDLE_VALUE) {
        CancelIoEx(hPipe, NULL);
    }

    if (m_captureThread.joinable()) {
        m_captureThread.join();
        LifecycleEvidence::Emit("AudioCaptureManager", "JOIN", m_threadId.ToString());
    }

    GetThreadManager().Unregister(m_threadId);
}

HANDLE AudioCaptureManager::CreateAudioPipe() {
    return CreateNamedPipeA(
        m_pipeName.c_str(),
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1,       // Max instances
        65536,   // Out buffer size
        65536,   // In buffer size
        0,       // Default timeout
        NULL
    );
}

void AudioCaptureManager::ClosePipeHandle(HANDLE hPipe) {
    if (hPipe != INVALID_HANDLE_VALUE) {
        CancelIoEx(hPipe, NULL);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

void AudioCaptureManager::CaptureLoop() {
    LOG(1, LogLevel::Info, LogCategory::Audio, "[AudioCaptureManager] Capture thread started.");

    
    //HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = hEvent;

    while (m_isRunning) {
        HANDLE currentPipe = CreateAudioPipe();
        if (currentPipe == INVALID_HANDLE_VALUE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        m_atomicPipeHandle.store(currentPipe);

        //ResetEvent(hEvent);
        BOOL connected = ConnectNamedPipe(currentPipe, &overlapped);
        if (!connected) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                while (m_isRunning) {
                    DWORD waitResult = WaitForSingleObject(hEvent, 30);
                    if (waitResult == WAIT_OBJECT_0) {
                        //connected = TRUE;
                        //break;
                        DWORD cbRet = 0;
                        // Xác thực kết nối thành công thực sự qua Overlapped Result
                        if (GetOverlappedResult(currentPipe, &overlapped, &cbRet, FALSE)) {
                            connected = TRUE;
                        }
                        break;
                    }
                    // Nếu Pipe bị ngắt/hỏng phía MPV trong lúc chờ, thoát ngay
                    if (GetLastError() == ERROR_BROKEN_PIPE || GetLastError() == ERROR_NO_DATA) {
                        break;
                    }
                }
            } else if (err == ERROR_PIPE_CONNECTED) {
                connected = TRUE;
            }
        }

        if (connected && m_isRunning) {
            LOG(1, LogLevel::Info, LogCategory::Audio, "[AudioCaptureManager] MPV connected to pipe.");

            UpdateFormatCacheFromState();
            const DWORD maxBytesToRead = static_cast<DWORD>(kMaxAudioSamples * sizeof(float));

            m_currentPts = 0.0;

            while (m_isRunning) {
                AudioBlock* writeSlot = m_producer->acquire_write();
                if (!writeSlot) {
                    m_metrics.ringOverflows.fetch_add(1, std::memory_order_relaxed);
                    m_metrics.blocksDropped.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }

                DWORD bytesRead = 0;
                ResetEvent(hEvent);

                BOOL success = ReadFile(
                    currentPipe,
                    writeSlot->samples.data(),
                    maxBytesToRead,
                    &bytesRead,
                    &overlapped
                );

                if (!success && GetLastError() == ERROR_IO_PENDING) {
                    while (m_isRunning) {
                        DWORD waitRes = WaitForSingleObject(hEvent, 10);
                        if (waitRes == WAIT_OBJECT_0) {
                            success = GetOverlappedResult(currentPipe, &overlapped, &bytesRead, FALSE);
                            break;
                        }
                    }
                }

                if (!m_isRunning || !success || bytesRead == 0) {
                    LOG(1, LogLevel::Warning, LogCategory::Audio, 
                        "[AudioCaptureManager] Pipe disconnected or stream read ended.");
                    break; 
                }

                if (m_isCapturing) {
                    UpdateFormatCacheFromState();

                    double timepos = 0.0;
                    bool isSeeking = false;
                    if (m_stateSystem) {
                        m_stateSystem->ReadPlayback([&timepos,&isSeeking](const PlaybackModel& model) {
                            timepos = model.timing.timePos;
                            isSeeking = model.flags.isSeeking;
                        });
                    }
                    if (isSeeking && !m_wasSeeking) {
                        m_activeGeneration++;
                    }
                    m_wasSeeking = isSeeking;

                    m_currentPts = timepos;
                    
                    const uint32_t sampleCount = bytesRead / static_cast<uint32_t>(sizeof(float));
                    
                    // Giới hạn channel tối đa kMaxAudioChannels (2)
                    uint32_t channels = m_formatCache.channels;
                    if (channels == 0 || channels > kMaxAudioChannels) {
                        channels = 2;
                    }
                    
                    const uint32_t sampleRate = (m_formatCache.sampleRate > 0) ? m_formatCache.sampleRate : 48000;

                    writeSlot->format.sampleRate = sampleRate;
                    writeSlot->format.channels = static_cast<uint8_t>(channels);
                    writeSlot->format.format = m_formatCache.format;

                    writeSlot->frames = sampleCount / channels;
                    writeSlot->sequence = m_sequence++;
                    writeSlot->generation = m_activeGeneration;
                    
                    writeSlot->pts = m_currentPts;

                    double blockDuration = static_cast<double>(writeSlot->frames) / static_cast<double>(sampleRate);
                    m_currentPts += blockDuration;

                    // Metrics Tracking
                    m_metrics.blocksReceived.fetch_add(1, std::memory_order_relaxed);
                    m_metrics.bytesReceived.fetch_add(bytesRead, std::memory_order_relaxed);
                    m_metrics.lastSequence.store(writeSlot->sequence, std::memory_order_relaxed);
                    m_metrics.lastPTS.store(writeSlot->pts, std::memory_order_relaxed);

                    m_producer->commit_write();
                }
            }
        }

        HANDLE hPipeToClose = m_atomicPipeHandle.exchange(INVALID_HANDLE_VALUE);
        ClosePipeHandle(hPipeToClose);
    }

    CloseHandle(hEvent);
    LOG(1, LogLevel::Info, LogCategory::Audio, "[AudioCaptureManager] Capture thread finished.");
}