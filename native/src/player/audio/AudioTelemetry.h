#pragma once

#include "AudioTypes.h"

#include <atomic>
#include <chrono>
#include <cstdint>

inline std::uint64_t AudioTelemetryNowMicros() noexcept
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

inline bool RecordFirstAudioTelemetry(
    std::atomic<std::uint64_t>& target,
    std::uint64_t timestampMicros) noexcept
{
    std::uint64_t expected = 0;
    return target.compare_exchange_strong(
        expected,
        timestampMicros,
        std::memory_order_relaxed,
        std::memory_order_relaxed);
}

inline double CanonicalQueuedAudioSeconds(std::uint32_t queuedBytes) noexcept
{
    const double bytesPerSecond =
        static_cast<double>(kCanonicalAudioSampleRate) *
        static_cast<double>(kCanonicalAudioChannels) *
        static_cast<double>(sizeof(float));
    return (bytesPerSecond > 0.0)
        ? (static_cast<double>(queuedBytes) / bytesPerSecond)
        : 0.0;
}

inline double EstimateAudibleHeadPts(
    double lastWrittenEndPts,
    std::uint32_t queuedBytes) noexcept
{
    return lastWrittenEndPts - CanonicalQueuedAudioSeconds(queuedBytes);
}

inline double EstimateAudioVideoOffsetSeconds(
    double lastWrittenEndPts,
    std::uint32_t queuedBytes,
    double mpvTimePos) noexcept
{
    return EstimateAudibleHeadPts(lastWrittenEndPts, queuedBytes) - mpvTimePos;
}
