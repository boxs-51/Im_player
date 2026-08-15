#pragma once

#include "AudioTypes.h"
#include "SpscRingBuffer.h"
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
 * @brief Thread CONSUMER cuối cùng trong pipeline âm thanh: 
 *        Đọc dữ liệu từ SpscRingBuffer<AudioBlock> (đã qua xử lý) 
 *        và xuất trực tiếp ra phần cứng (SDL2 / WASAPI).
 */
class AudioOutputWorker {
public:
    AudioOutputWorker();
    ~AudioOutputWorker();

    AudioOutputWorker(const AudioOutputWorker&) = delete;
    AudioOutputWorker& operator=(const AudioOutputWorker&) = delete;

    /**
     * @brief Khởi tạo Worker với RingBuffer và StateSystem
     */
    bool Init(SpscRingBuffer<AudioBlock>* processedStream,
              AudioBackendType backend = AudioBackendType::SDL2);

    void Start();
    void Stop();

    /**
     * @brief Chuyển đổi linh hoạt Backend âm thanh ngay tại Runtime
     */
    bool SwitchBackend(AudioBackendType newBackend);

    /**
     * @brief Kiểm tra xem Worker có đang chạy hay không
     */
    bool IsRunning() const { return m_isRunning.load(std::memory_order_relaxed); }

private:
    void OutputLoop();
    std::unique_ptr<IAudioOutputDevice> CreateDeviceBackend(AudioBackendType type);

    // --- References & Streams ---
    SpscRingBuffer<AudioBlock>* m_processedStream = nullptr;

    // --- Audio Backend ---
    std::unique_ptr<IAudioOutputDevice> m_audioDevice;
    AudioBackendType m_currentBackendType = AudioBackendType::SDL2;

    // --- Thread Control ---
    std::thread m_workerThread;
    std::atomic<bool> m_isRunning{false};
    uint64_t m_lastGeneration = 0;
    ThreadID m_threadId;
};