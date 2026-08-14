#pragma once

#include "AudioCaptureManager.h"
#include "PlayerStateSystem.h"
#include "IAudioOutputDevice.h"
#include <memory>
#include <thread>
#include <atomic>
#include <string>

enum class AudioBackendType {
    SDL2,
    WASAPI,
    ASIO
};

class AudioOutputWorker {
public:
    AudioOutputWorker();
    ~AudioOutputWorker();

    bool Init(AudioCaptureManager* captureManager, PlayerStateSystem* stateSystem, AudioBackendType backend = AudioBackendType::SDL2);
    void Start();
    void Stop();

    // Cho phép đổi Backend linh hoạt lúc đang chạy (Runtime Backend Switching)
    bool SwitchBackend(AudioBackendType newBackend);

private:
    void OutputLoop();
    std::unique_ptr<IAudioOutputDevice> CreateDeviceBackend(AudioBackendType type);

    AudioCaptureManager* m_captureManager = nullptr;
    PlayerStateSystem*   m_stateSystem = nullptr;

    std::unique_ptr<IAudioOutputDevice> m_audioDevice;
    AudioBackendType m_currentBackendType = AudioBackendType::SDL2;

    std::thread m_workerThread;
    std::atomic<bool> m_isRunning{false};
    uint64_t m_lastGeneration = 0;
    std::string m_threadId;
};