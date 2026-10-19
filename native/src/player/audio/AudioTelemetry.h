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

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    const DWORD length = static_cast<DWORD>(
        lineWritten < static_cast<int>(sizeof(line))
            ? lineWritten
            : sizeof(line) - 1);
    DWORD bytesWritten = 0;
    WriteFile(file, line, length, &bytesWritten, nullptr);
    CloseHandle(file);
}

inline bool AudioStartupBoundaryTraceEnabled() noexcept
{
    static const bool enabled = []() noexcept {
        const char* value = std::getenv("IM_PLAYER_STARTUP_BOUNDARY_TRACE");
        return value && *value && *value != '0';
    }();
    return enabled;
}

inline void EmitStartupBoundaryEvidence(const char* format, ...)
{
    if (!AudioStartupBoundaryTraceEnabled() || !format)
        return;

    char payload[1536]{};
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(payload, sizeof(payload), format, args);
    va_end(args);
    if (written <= 0)
        return;

    EmitAudioTelemetryEvidence("STARTUP_BOUNDARY %s", payload);
}

inline bool AudioContinuityTraceEnabled() noexcept
{
    static const bool enabled = []() noexcept {
        const char* value = std::getenv("IM_PLAYER_AUDIO_CONTINUITY_TRACE");
        return value && *value && *value != '0';
    }();
    return enabled;
}

inline void EmitAudioContinuityEvidence(const char* format, ...)
{
    if (!AudioContinuityTraceEnabled() || !format)
        return;

    char payload[1536]{};
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(payload, sizeof(payload), format, args);
    va_end(args);
    if (written <= 0)
        return;

    EmitAudioTelemetryEvidence("CONTINUITY %s", payload);
}

inline bool AudioBackpressureTraceEnabled() noexcept
{
    static const bool enabled = []() noexcept {
        const char* value = std::getenv("IM_PLAYER_AUDIO_BACKPRESSURE_TRACE");
        return value && *value && *value != '0';
    }();
    return enabled;
}

inline void EmitAudioBackpressureEvidence(const char* format, ...)
{
    if (!AudioBackpressureTraceEnabled() || !format)
        return;

    char payload[1536]{};
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(payload, sizeof(payload), format, args);
    va_end(args);
    if (written <= 0)
        return;

    EmitAudioTelemetryEvidence("BACKPRESSURE %s", payload);
}
