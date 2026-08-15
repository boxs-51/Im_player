#pragma once

#include "AudioProcessor.h"
#include "PlayerStateSystem.h"
#include "IAudioOutputDevice.h"
#include "threads/thread_id.h"
#include <memory>
#include <thread>
#include <atomic>
#include <string>

enum class AudioBackendType {
    SDL2,
    WASAPI,
    ASIO
};

/**
 * @class AudioOutputWorker
 * @brief Thread CONSUMER cuối cùng trong pipeline: Nhận data từ AudioProcessor 
 *        và xuất ra thiết bị phần cứng (SDL2 / WASAPI).
 */
class AudioOutputWorker {
public:
    AudioOutputWorker();
    ~AudioOutputWorker();

    AudioOutputWorker(const AudioOutputWorker&) = delete;
    AudioOutputWorker& operator=(const AudioOutputWorker&) = delete;

    // Thay đổi tham chiếu đầu vào sang AudioProcessor để nhận stream đã qua xử lý
    bool Init(SpscRingBuffer<AudioBlock>* processedStream, PlayerStateSystem* stateSystem, AudioBackendType backend = AudioBackendType::SDL2);
    void Start();
    void Stop();

    // Cho phép chuyển đổi linh hoạt Backend âm thanh lúc runtime
    bool SwitchBackend(AudioBackendType newBackend);

private:
    void OutputLoop();
    std::unique_ptr<IAudioOutputDevice> CreateDeviceBackend(AudioBackendType type);

    SpscRingBuffer<AudioBlock>* m_processedStream = nullptr;
    PlayerStateSystem*   m_stateSystem = nullptr;

    std::unique_ptr<IAudioOutputDevice> m_audioDevice;
    AudioBackendType m_currentBackendType = AudioBackendType::SDL2;

    std::thread m_workerThread;
    std::atomic<bool> m_isRunning{false};
    uint64_t m_lastGeneration = 0;
    ThreadID m_threadId;
};