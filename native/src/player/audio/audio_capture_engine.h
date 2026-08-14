#pragma once

#include "ring_buffer.h"
#include <string>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

class AudioCaptureEngine {
public:
    // Sử dụng mẫu audio là float, kích thước buffer 48000 (tương đương 1 giây ở 48kHz)
    AudioCaptureEngine();
    ~AudioCaptureEngine();

    bool Start();
    void Stop();

    std::string GetPipeName() const;
    ThreadSafeRingBuffer<float>& GetBuffer();

private:
    void CaptureLoop();

    ThreadSafeRingBuffer<float> m_buffer;
    std::thread m_captureThread;
    std::atomic<bool> m_isRunning;
    std::string m_pipeName;
    HANDLE m_pipeHandle = INVALID_HANDLE_VALUE;
};