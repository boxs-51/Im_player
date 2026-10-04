#pragma once

#include "AudioTypes.h"

#include <algorithm>
#include <cstdint>

struct AudioQueueWritePlan {
    std::uint32_t framesToWrite = 0;
    std::uint32_t roomBytes = 0;
};

/**
 * @brief Compute a frame-aligned write segment that cannot exceed the
 * Issue #26 SDL queue hard cap.
 */
inline constexpr AudioQueueWritePlan PlanAudioQueueWrite(
    std::uint32_t queuedBytes,
    std::uint32_t remainingFrames) noexcept
{
    if (remainingFrames == 0 ||
        queuedBytes >= kAudioOutputDesignMaxQueueBytes) {
        return {};
    }

    const std::uint32_t roomBytes =
        kAudioOutputDesignMaxQueueBytes - queuedBytes;
    const std::uint32_t roomFrames =
        roomBytes / static_cast<std::uint32_t>(
            kCanonicalAudioBytesPerFrame);

    return {
        std::min(remainingFrames, roomFrames),
        roomBytes,
    };
}
