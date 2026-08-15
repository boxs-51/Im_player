#pragma once

#include "IAudioOutputDevice.h"
#include <SDL.h>
#include <atomic>
#include <mutex>

class SdlAudioDevice : public IAudioOutputDevice {
public:
    SdlAudioDevice();
    ~SdlAudioDevice() override;

    bool Open(uint32_t sampleRate, uint8_t channels) override;
    void Close() override;
    void Write(const float* samples, size_t sampleCount) override;
    void FlushBuffers() override;
    uint32_t GetQueuedSizeBytes() const override;

    // --- Phương thức quản lý trạng thái thiết bị mới ---
    bool IsReady() const override;
    void SetReady(bool ready) override;
    void Shutdown() override;

private:
    std::atomic<SDL_AudioDeviceID> m_deviceId{0};
    std::atomic<bool> m_isSdlAudioInitialized{false};
    std::atomic<bool> m_isReady{false};
    mutable std::mutex m_lifecycleMutex;
};