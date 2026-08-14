#pragma once
#include "IAudioOutputDevice.h"

// Dễ dàng chèn WASAPI IAudioClient backend tại đây mà không sửa 1 dòng code nào ở AudioOutputWorker
class WasapiAudioDevice : public IAudioOutputDevice {
public:
    bool Open(uint32_t sampleRate, uint8_t channels) override {
        // Init COM & WASAPI IAudioClient
        return true;
    }
    void Close() override {}
    void Write(const float* samples, size_t sampleCount) override {}
    void FlushBuffers() override {}
    uint32_t GetQueuedSizeBytes() const override { return 0; }
};