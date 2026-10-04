#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

// Định dạng mẫu âm thanh (Mặc định Float32 để khớp với MPV f32le)
enum class AudioSampleFormat : uint16_t {
    Float32 = 0,
    Int16   = 1
};

// Cấu hình định dạng Audio Stream
struct AudioFormat {
    uint32_t sampleRate = 48000;
    uint16_t channels   = 2;
    AudioSampleFormat format = AudioSampleFormat::Float32;
};

// Khai báo hằng số kích thước khung Audio
constexpr size_t kMaxAudioFrames   = 2048; // ~42.67ms tại 48kHz
constexpr size_t kMaxAudioChannels = 2;    // Stereo
constexpr size_t kMaxAudioSamples  = kMaxAudioFrames * kMaxAudioChannels; // 4096 floats

// Issue #26 canonical PCM transport/output contract.
constexpr uint32_t kCanonicalAudioSampleRate = 48000;
constexpr uint16_t kCanonicalAudioChannels = 2;
constexpr size_t kCanonicalAudioBytesPerFrame =
    sizeof(float) * static_cast<size_t>(kCanonicalAudioChannels);
constexpr uint32_t kCanonicalAudioBytesPerSecond =
    kCanonicalAudioSampleRate *
    static_cast<uint32_t>(kCanonicalAudioBytesPerFrame);

// Issue #26 external SDL jitter reservoir.
// Keep ~100 ms queued under steady playback. A single maximum AudioBlock can
// add at most ~42.67 ms, so target + one block remains below 150 ms.
constexpr uint32_t kAudioOutputTargetQueueMilliseconds = 100;
constexpr uint32_t kAudioOutputTargetQueueBytes =
    (kCanonicalAudioBytesPerSecond * kAudioOutputTargetQueueMilliseconds) / 1000;
constexpr uint32_t kAudioOutputMaxBlockBytes =
    static_cast<uint32_t>(kMaxAudioFrames * kCanonicalAudioBytesPerFrame);
constexpr uint32_t kAudioOutputDesignMaxQueueMilliseconds = 150;
constexpr uint32_t kAudioOutputDesignMaxQueueBytes =
    (kCanonicalAudioBytesPerSecond * kAudioOutputDesignMaxQueueMilliseconds) / 1000;
static_assert(
    kAudioOutputTargetQueueBytes + kAudioOutputMaxBlockBytes <=
        kAudioOutputDesignMaxQueueBytes,
    "Issue #26 queue target plus one maximum block must remain <=150ms");

/**
 * @struct AudioBlock
 * @brief Đơn vị dữ liệu âm thanh chuyển giao giữa các thread trong Audio Pipeline.
 * Giúp chuyển đổi từ sample-oriented (float lẻ) sang block-oriented (~23 blocks/giây).
 */
struct AudioBlock {
    uint64_t sequence   = 0;   // Số thứ tự block để kiểm tra continuity
    uint64_t generation = 0;   // Dùng để reset/flush audio cũ khi Seek / Track Change
    double   pts        = 0.0; // Presentation Timestamp (giây) hỗ trợ STT / Subtitle sync

    AudioFormat format;
    uint32_t frames     = 0;   // Số frames thực tế trong block (tối đa kMaxAudioFrames)

    // Storage cố định để tránh allocation trong capture thread/fast path
    alignas(16) std::array<float, kMaxAudioSamples> samples{};

    // Trả về số lượng float samples (frames * channels)
    size_t sample_count() const noexcept {
        return static_cast<size_t>(frames) * format.channels;
    }

    // Trả về thời lượng block tính bằng giây
    double duration_seconds() const noexcept {
        return (format.sampleRate > 0) ? (static_cast<double>(frames) / format.sampleRate) : 0.0;
    }
};

/**
 * @struct AudioPipelineMetrics
 * @brief Lưu trữ các thông số đo đạc hiệu năng & debug audio pipeline.
 */
struct AudioPipelineMetrics {
    uint64_t blocksReceived  = 0;
    uint64_t blocksDropped   = 0;
    uint64_t bytesReceived   = 0;

    uint64_t ringOverflows   = 0;
    uint64_t ringUnderflows  = 0;
    uint64_t backpressureWaits = 0;
    uint64_t pacingSleepCount = 0;
    uint64_t pacingSleepMicros = 0;
    uint64_t pacingRebases = 0;

    uint64_t lastSequence    = 0;
    uint64_t currentGeneration = 0;
    uint64_t partialFrameCarryBytes = 0;
    uint64_t firstPipeConnectedMicros = 0;
    uint64_t firstPipeBytesMicros = 0;
    uint64_t firstCompletePcmBlockMicros = 0;
    double   lastPTS         = 0.0;
};

/**
 * @struct AudioOutputMetrics
 * @brief Observable SDL output/queue metrics for the canonical PCM contract.
 */
struct AudioOutputMetrics {
    uint64_t blocksWritten = 0;
    uint64_t blocksDroppedFormatMismatch = 0;
    uint64_t queueUnderflowEvents = 0;
    uint64_t writeFailures = 0;
    uint64_t firstProcessedBlockMicros = 0;
    uint64_t firstSdlWriteMicros = 0;
    uint64_t firstNonzeroQueueMicros = 0;
    uint32_t queuedBytes = 0;
    uint32_t queueHighWaterBytes = 0;
    double queuedMilliseconds = 0.0;
    double lastWrittenPts = 0.0;
    double lastWrittenEndPts = 0.0;
    double mpvTimePos = 0.0;
    double estimatedAudibleHeadPts = 0.0;
    double estimatedAvOffsetSeconds = 0.0;
    uint64_t syncSampleCount = 0;
    uint64_t firstSyncSampleMicros = 0;
    uint64_t lastSyncSampleMicros = 0;
    double firstSyncAvOffsetSeconds = 0.0;
    double lastSyncAvOffsetSeconds = 0.0;
    double maxAbsAvOffsetSeconds = 0.0;
    uint32_t sampleRate = kCanonicalAudioSampleRate;
    uint16_t channels = kCanonicalAudioChannels;
};