#pragma once

#include "AudioTypes.h"
#include "SpscRingBuffer.h"
#include "PlayerStateSystem.h"
#include "IAudioOutputDevice.h"
#include "threads/thread_id.h"

#include <optional>
#include <memory>
#include <thread>
#include <atomic>
#include <string>

enum class AudioBackendType {
    SDL2,
    WASAPI,
    ASIO
};

struct AudioOutputMetricsAtomic {
    std::atomic<uint64_t> blocksWritten{0};
    std::atomic<uint64_t> blocksDroppedFormatMismatch{0};
    std::atomic<uint64_t> queueUnderflowEvents{0};
    std::atomic<uint64_t> writeFailures{0};
    std::atomic<uint32_t> queuedBytes{0};
    std::atomic<uint32_t> queueHighWaterBytes{0};
    std::atomic<double> queuedMilliseconds{0.0};
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
    bool Init(SpscConsumer<AudioBlock> processedStream,
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

    AudioOutputMetrics GetMetrics() const;

private:
    void OutputLoop();
    std::unique_ptr<IAudioOutputDevice> CreateDeviceBackend(AudioBackendType type);

    // --- References & Streams ---
    std::optional<SpscConsumer<AudioBlock>> m_processedStream;

    // --- Audio Backend ---
    std::unique_ptr<IAudioOutputDevice> m_audioDevice;
    AudioBackendType m_currentBackendType = AudioBackendType::SDL2;

    // --- Thread Control ---
    std::thread m_workerThread;
    std::atomic<bool> m_isRunning{false};
    uint64_t m_lastGeneration = 0;
    ThreadID m_threadId = "";
    AudioOutputMetricsAtomic m_metrics;
};