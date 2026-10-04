#include "AudioCaptureManager.h"
#include "PcmFrameAccumulator.h"
#include "PcmRealtimePacer.h"
#include "AudioTelemetry.h"
#include "PlayerStateSystem.h"
#include "log.h"
#include "threads/thread_manager.h"
#include "common/LifecycleEvidence.h"
#include <iostream>
#include <algorithm>
#include <array>
#include <cstdio>

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


bool AudioCaptureManager::SetRequiredMpvProperty(const char* name, const char* value) {
    const int result = mpv_set_property_string(m_mpv, name, value);
    if (result < 0) {
        LOG(1, LogLevel::Error, LogCategory::Audio,
            "[AudioCaptureManager] Required MPV audio property failed: %s=%s error=%s (%d)",
            name, value, mpv_error_string(result), result);
        return false;
    }

    LOG(1, LogLevel::Info, LogCategory::Audio,
        "[AudioCaptureManager] PCM transport property: %s=%s", name, value);
    EmitAudioTelemetryEvidence(
        "MPV_PROPERTY_SET name=%s value=%s result=%d",
        name,
        value,
        result);
    return true;
}

void AudioCaptureManager::LogEffectiveMpvProperty(const char* name) const {
    char* value = mpv_get_property_string(m_mpv, name);
    if (!value) {
        LOG(1, LogLevel::Warning, LogCategory::Audio,
            "[AudioCaptureManager] Unable to read effective MPV property: %s", name);
        return;
    }

    LOG(1, LogLevel::Info, LogCategory::Audio,
        "[AudioCaptureManager] Effective MPV audio property: %s=%s", name, value);
    EmitAudioTelemetryEvidence(
        "MPV_PROPERTY_EFFECTIVE name=%s value=%s",
        name,
        value);
    mpv_free(value);
}

bool AudioCaptureManager::ConfigureMpvPcmTransport() {
    // Configure the payload contract before selecting the PCM AO. The capture
    // thread consumes the pipe as raw native-endian float32 stereo frames.
    const bool configured =
        SetRequiredMpvProperty("ao-pcm-waveheader", "no")
        && SetRequiredMpvProperty("audio-format", "float")
        && SetRequiredMpvProperty("audio-samplerate", "48000")
        && SetRequiredMpvProperty("audio-channels", "stereo")
        && SetRequiredMpvProperty("ao-pcm-file", m_pipeName.c_str())
        && SetRequiredMpvProperty("ao", "pcm");

    if (!configured)
        return false;

    for (const char* name : {
             "ao",
             "ao-pcm-waveheader",
             "audio-format",
             "audio-samplerate",
             "audio-channels",
             "ao-pcm-file",
         }) {
        LogEffectiveMpvProperty(name);
    }
    return true;
}

AudioPipelineMetrics AudioCaptureManager::GetMetrics() const {
    AudioPipelineMetrics snapshot;
    snapshot.blocksReceived = m_metrics.blocksReceived.load(std::memory_order_relaxed);
    snapshot.blocksDropped = m_metrics.blocksDropped.load(std::memory_order_relaxed);
    snapshot.bytesReceived = m_metrics.bytesReceived.load(std::memory_order_relaxed);
    snapshot.ringOverflows = m_metrics.ringOverflows.load(std::memory_order_relaxed);
    snapshot.backpressureWaits =
        m_metrics.backpressureWaits.load(std::memory_order_relaxed);
    snapshot.pacingSleepCount =
        m_metrics.pacingSleepCount.load(std::memory_order_relaxed);
    snapshot.pacingSleepMicros =
        m_metrics.pacingSleepMicros.load(std::memory_order_relaxed);
    snapshot.pacingRebases =
        m_metrics.pacingRebases.load(std::memory_order_relaxed);
    snapshot.lastSequence = m_metrics.lastSequence.load(std::memory_order_relaxed);
    snapshot.currentGeneration = m_metrics.currentGeneration.load(std::memory_order_relaxed);
    snapshot.partialFrameCarryBytes = m_metrics.partialFrameCarryBytes.load(std::memory_order_relaxed);
    snapshot.firstPipeConnectedMicros =
        m_metrics.firstPipeConnectedMicros.load(std::memory_order_relaxed);
    snapshot.firstPipeBytesMicros =
        m_metrics.firstPipeBytesMicros.load(std::memory_order_relaxed);
    snapshot.firstCompletePcmBlockMicros =
        m_metrics.firstCompletePcmBlockMicros.load(std::memory_order_relaxed);
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

    StartCapture();

    if (!ConfigureMpvPcmTransport()) {
        LOG(1, LogLevel::Error, LogCategory::Audio,
            "[AudioCaptureManager] Canonical PCM transport setup failed; capture startup aborted.");
        StopCapture();
        m_producer.reset();
        m_stateSystem = nullptr;
        m_mpv = nullptr;
        return false;
    }

    LOG(1, LogLevel::Info, LogCategory::Audio,
        "[AudioCaptureManager] Canonical PCM contract established: raw float32, 48000 Hz, stereo, %zu bytes/frame.",
        kCanonicalAudioBytesPerFrame);
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
    m_metrics.blocksReceived.store(0, std::memory_order_relaxed);
    m_metrics.blocksDropped.store(0, std::memory_order_relaxed);
    m_metrics.bytesReceived.store(0, std::memory_order_relaxed);
    m_metrics.ringOverflows.store(0, std::memory_order_relaxed);
    m_metrics.backpressureWaits.store(0, std::memory_order_relaxed);
    m_metrics.pacingSleepCount.store(0, std::memory_order_relaxed);
    m_metrics.pacingSleepMicros.store(0, std::memory_order_relaxed);
    m_metrics.pacingRebases.store(0, std::memory_order_relaxed);
    m_metrics.firstPipeConnectedMicros.store(0, std::memory_order_relaxed);
    m_metrics.firstPipeBytesMicros.store(0, std::memory_order_relaxed);
    m_metrics.firstCompletePcmBlockMicros.store(0, std::memory_order_relaxed);

    m_captureThread = std::thread(&AudioCaptureManager::CaptureLoop, this);
    GetThreadManager().Register(m_threadId, &m_captureThread);
    LifecycleEvidence::Emit("AudioCaptureManager", "START", LifecycleEvidence::PointerIdentity(this));
}

void AudioCaptureManager::StopCapture() {
    if (!m_isRunning.exchange(false)) {
        return;
    }

    LifecycleEvidence::Emit("AudioCaptureManager", "STOP", LifecycleEvidence::PointerIdentity(this));
    m_isCapturing = false;

    HANDLE hPipe = m_atomicPipeHandle.load();
    if (hPipe != INVALID_HANDLE_VALUE) {
        CancelIoEx(hPipe, NULL);
    }

    if (m_captureThread.joinable()) {
        m_captureThread.join();
        LifecycleEvidence::Emit("AudioCaptureManager", "JOIN", LifecycleEvidence::PointerIdentity(this));
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
            const std::uint64_t pipeConnectedMicros = AudioTelemetryNowMicros();
            if (RecordFirstAudioTelemetry(
                    m_metrics.firstPipeConnectedMicros,
                    pipeConnectedMicros)) {
                LOG(1, LogLevel::Info, LogCategory::Audio,
                    "[AUDIO_TIMELINE] PIPE_CONNECTED t_us=%llu",
                    static_cast<unsigned long long>(pipeConnectedMicros));
                EmitAudioTelemetryEvidence(
                    "PIPE_CONNECTED t_us=%llu",
                    static_cast<unsigned long long>(pipeConnectedMicros));
            }

            LOG(1, LogLevel::Info, LogCategory::Audio, "[AudioCaptureManager] MPV connected to pipe.");

            PcmFrameAccumulator accumulator;
            PcmRealtimePacer realtimePacer;
            std::array<std::uint8_t, PcmFrameAccumulator::kReadCapacity> readBuffer{};
            bool firstPayloadLogged = false;
            bool firstPacingDelayLogged = false;
            m_currentPts = 0.0;
            m_metrics.partialFrameCarryBytes.store(0, std::memory_order_relaxed);

            while (m_isRunning) {
                if (!m_isCapturing) {
                    if (accumulator.PendingBytes() != 0) {
                        accumulator.Reset();
                        m_metrics.partialFrameCarryBytes.store(0, std::memory_order_relaxed);
                    }
                    realtimePacer.Reset();
                }

                if (accumulator.ReadyFrames() == 0) {
                    DWORD bytesRead = 0;
                    ResetEvent(hEvent);

                    BOOL success = ReadFile(
                        currentPipe,
                        readBuffer.data(),
                        static_cast<DWORD>(readBuffer.size()),
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

                    m_metrics.bytesReceived.fetch_add(bytesRead, std::memory_order_relaxed);

                    const std::uint64_t pipeBytesMicros = AudioTelemetryNowMicros();
                    if (RecordFirstAudioTelemetry(
                            m_metrics.firstPipeBytesMicros,
                            pipeBytesMicros)) {
                        LOG(1, LogLevel::Info, LogCategory::Audio,
                            "[AUDIO_TIMELINE] FIRST_PIPE_BYTES t_us=%llu bytes=%lu",
                            static_cast<unsigned long long>(pipeBytesMicros),
                            static_cast<unsigned long>(bytesRead));
                        EmitAudioTelemetryEvidence(
                            "FIRST_PIPE_BYTES t_us=%llu bytes=%lu",
                            static_cast<unsigned long long>(pipeBytesMicros),
                            static_cast<unsigned long>(bytesRead));
                    }

                    if (!firstPayloadLogged) {
                        char preview[3 * 8 + 1] = {};
                        const size_t previewBytes = std::min<size_t>(bytesRead, 8);
                        size_t offset = 0;
                        for (size_t index = 0; index < previewBytes; ++index) {
                            offset += static_cast<size_t>(std::snprintf(
                                preview + offset,
                                sizeof(preview) - offset,
                                "%02X%s",
                                static_cast<unsigned int>(readBuffer[index]),
                                (index + 1 < previewBytes) ? " " : ""));
                        }
                        LOG(1, LogLevel::Info, LogCategory::Audio,
                            "[AudioCaptureManager] First raw PCM payload bytes=%lu preview=[%s] contract=float32/48000/stereo/raw",
                            static_cast<unsigned long>(bytesRead), preview);
                        EmitAudioTelemetryEvidence(
                            "FIRST_RAW_PCM_PAYLOAD bytes=%lu preview=[%s] contract=float32/48000/stereo/raw",
                            static_cast<unsigned long>(bytesRead),
                            preview);
                        firstPayloadLogged = true;
                    }

                    if (!accumulator.Append(readBuffer.data(), bytesRead)) {
                        LOG(1, LogLevel::Error, LogCategory::Audio,
                            "[AudioCaptureManager] PCM byte accumulator overflow; refusing ambiguous framing.");
                        EmitAudioTelemetryEvidence(
                            "ERROR PCM_ACCUMULATOR_OVERFLOW pending_bytes=%llu append_bytes=%lu",
                            static_cast<unsigned long long>(accumulator.PendingBytes()),
                            static_cast<unsigned long>(bytesRead));
                        break;
                    }

                    m_metrics.partialFrameCarryBytes.store(
                        accumulator.PendingBytes() % PcmFrameAccumulator::kFrameBytes,
                        std::memory_order_relaxed);

                    if (!m_isCapturing) {
                        accumulator.Reset();
                        m_metrics.partialFrameCarryBytes.store(0, std::memory_order_relaxed);
                        continue;
                    }
                }

                if (accumulator.ReadyFrames() == 0) {
                    continue;
                }

                const PcmPacingDecision pacing =
                    realtimePacer.BeforePublish(AudioTelemetryNowMicros());
                if (pacing.rebased) {
                    m_metrics.pacingRebases.fetch_add(1, std::memory_order_relaxed);
                    EmitAudioTelemetryEvidence(
                        "PCM_PACER_REBASE published_frames=%llu",
                        static_cast<unsigned long long>(
                            realtimePacer.PublishedFrames()));
                }
                if (pacing.delayMicros > 0) {
                    m_metrics.pacingSleepCount.fetch_add(1, std::memory_order_relaxed);
                    m_metrics.pacingSleepMicros.fetch_add(
                        pacing.delayMicros,
                        std::memory_order_relaxed);
                    if (!firstPacingDelayLogged) {
                        EmitAudioTelemetryEvidence(
                            "PCM_PACING_ACTIVE delay_us=%llu lead_frames=%llu",
                            static_cast<unsigned long long>(pacing.delayMicros),
                            static_cast<unsigned long long>(
                                PcmRealtimePacer::kLeadFrames));
                        firstPacingDelayLogged = true;
                    }
                    std::this_thread::sleep_for(
                        std::chrono::microseconds(pacing.delayMicros));
                    if (!m_isRunning)
                        break;
                }

                AudioBlock* writeSlot = m_producer->acquire_write();
                while (!writeSlot && m_isRunning.load(std::memory_order_relaxed)) {
                    // This is backpressure, not data loss: accumulator bytes
                    // remain owned by CaptureThread until a raw-ring slot exists.
                    m_metrics.backpressureWaits.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    writeSlot = m_producer->acquire_write();
                }
                if (!m_isRunning || !writeSlot)
                    break;

                const uint32_t frames = accumulator.DrainFrames(
                    writeSlot->samples.data(),
                    static_cast<uint32_t>(kMaxAudioFrames));

                if (frames == 0) {
                    continue;
                }

                const uint64_t carryBytes =
                    accumulator.PendingBytes() % PcmFrameAccumulator::kFrameBytes;
                m_metrics.partialFrameCarryBytes.store(
                    carryBytes,
                    std::memory_order_relaxed);

                const std::uint64_t completeBlockMicros = AudioTelemetryNowMicros();
                if (RecordFirstAudioTelemetry(
                        m_metrics.firstCompletePcmBlockMicros,
                        completeBlockMicros)) {
                    LOG(1, LogLevel::Info, LogCategory::Audio,
                        "[AUDIO_TIMELINE] FIRST_COMPLETE_PCM_BLOCK t_us=%llu frames=%u carry_bytes=%llu",
                        static_cast<unsigned long long>(completeBlockMicros),
                        frames,
                        static_cast<unsigned long long>(carryBytes));
                    EmitAudioTelemetryEvidence(
                        "FIRST_COMPLETE_PCM_BLOCK t_us=%llu frames=%u carry_bytes=%llu sample_rate=%u channels=%u format=float32",
                        static_cast<unsigned long long>(completeBlockMicros),
                        frames,
                        static_cast<unsigned long long>(carryBytes),
                        static_cast<unsigned int>(kCanonicalAudioSampleRate),
                        static_cast<unsigned int>(kCanonicalAudioChannels));
                }

                double timepos = 0.0;
                bool isSeeking = false;
                if (m_stateSystem) {
                    m_stateSystem->ReadPlayback([&timepos, &isSeeking](const PlaybackModel& model) {
                        timepos = model.timing.timePos;
                        isSeeking = model.flags.isSeeking;
                    });
                }
                if (isSeeking && !m_wasSeeking) {
                    m_activeGeneration++;
                    realtimePacer.Reset();
                    EmitAudioTelemetryEvidence(
                        "PCM_PACER_RESET reason=seek generation=%llu",
                        static_cast<unsigned long long>(m_activeGeneration));
                }
                m_wasSeeking = isSeeking;

                m_currentPts = timepos;

                writeSlot->format.sampleRate = kCanonicalAudioSampleRate;
                writeSlot->format.channels = kCanonicalAudioChannels;
                writeSlot->format.format = AudioSampleFormat::Float32;
                writeSlot->frames = frames;
                writeSlot->sequence = m_sequence++;
                writeSlot->generation = m_activeGeneration;
                writeSlot->pts = m_currentPts;

                const double blockDuration =
                    static_cast<double>(writeSlot->frames) /
                    static_cast<double>(kCanonicalAudioSampleRate);
                m_currentPts += blockDuration;

                m_metrics.blocksReceived.fetch_add(1, std::memory_order_relaxed);
                m_metrics.lastSequence.store(writeSlot->sequence, std::memory_order_relaxed);
                m_metrics.currentGeneration.store(writeSlot->generation, std::memory_order_relaxed);
                m_metrics.lastPTS.store(writeSlot->pts, std::memory_order_relaxed);

                m_producer->commit_write();
                realtimePacer.OnPublished(frames);
            }
        }

        HANDLE hPipeToClose = m_atomicPipeHandle.exchange(INVALID_HANDLE_VALUE);
        ClosePipeHandle(hPipeToClose);
    }

    CloseHandle(hEvent);
    LOG(1, LogLevel::Info, LogCategory::Audio, "[AudioCaptureManager] Capture thread finished.");
}