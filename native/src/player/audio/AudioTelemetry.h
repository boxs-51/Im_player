#pragma once

#include "AudioTypes.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

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

inline std::mutex& AudioTelemetryEvidenceMutex()
{
    static std::mutex mutex;
    return mutex;
}

inline void EmitAudioTelemetryEvidence(const char* format, ...)
{
    if (!format)
        return;

    char payload[1536]{};
    va_list args;
    va_start(args, format);
    const int payloadWritten = std::vsnprintf(payload, sizeof(payload), format, args);
    va_end(args);
    if (payloadWritten <= 0)
        return;

    char line[1792]{};
    const int lineWritten = std::snprintf(
        line,
        sizeof(line),
        "[AUDIO-TELEMETRY] %s\n",
        payload);
    if (lineWritten <= 0)
        return;

    OutputDebugStringA(line);

    const char* path = std::getenv("IM_PLAYER_LIFECYCLE_LOG");
    if (!path || !*path)
        return;

    std::lock_guard<std::mutex> lock(AudioTelemetryEvidenceMutex());

    FILE* file = nullptr;
    if (fopen_s(&file, path, "ab") != 0 || !file)
        return;

    const size_t length = static_cast<size_t>(
        lineWritten < static_cast<int>(sizeof(line))
            ? lineWritten
            : sizeof(line) - 1);
    std::fwrite(line, 1, length, file);
    std::fclose(file);
}
