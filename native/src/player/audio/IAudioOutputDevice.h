#pragma once

#include <cstdint>
#include <stdint.h>

/**
 * @brief Interface trừu tượng cho tất cả các Audio Output Hardware Drivers (SDL2, WASAPI, ASIO...)
 */
class IAudioOutputDevice {
public:
    virtual ~IAudioOutputDevice() = default;

    virtual bool Open(uint32_t sampleRate, uint8_t channels) = 0;
    virtual void Close() = 0;
    virtual void Write(const float* samples, size_t sampleCount) = 0;
    virtual void FlushBuffers() = 0; // Clear hardware queued audio khi seek / state change
    virtual uint32_t GetQueuedSizeBytes() const = 0;
};