#include "player/audio/AudioQueueWritePlanner.h"

#include <iostream>

int main()
{
    {
        const auto plan = PlanAudioQueueWrite(0, 2048);
        if (plan.framesToWrite != 2048) {
            std::cerr << "empty queue should accept full max block\n";
            return 1;
        }
    }

    {
        const auto plan = PlanAudioQueueWrite(
            kAudioOutputTargetQueueBytes,
            2048);
        const std::uint32_t maxFrames =
            (kAudioOutputDesignMaxQueueBytes -
             kAudioOutputTargetQueueBytes) /
            static_cast<std::uint32_t>(kCanonicalAudioBytesPerFrame);
        if (plan.framesToWrite != maxFrames) {
            std::cerr << "target queue write must be segmented to hard-cap room\n";
            return 2;
        }
    }

    {
        const auto plan = PlanAudioQueueWrite(
            kAudioOutputDesignMaxQueueBytes,
            2048);
        if (plan.framesToWrite != 0) {
            std::cerr << "hard-cap queue must reject additional frames\n";
            return 3;
        }
    }

    {
        const std::uint32_t oneFrameRoom =
            kAudioOutputDesignMaxQueueBytes -
            static_cast<std::uint32_t>(kCanonicalAudioBytesPerFrame);
        const auto plan = PlanAudioQueueWrite(oneFrameRoom, 2048);
        if (plan.framesToWrite != 1) {
            std::cerr << "planner must preserve frame alignment\n";
            return 4;
        }
    }

    {
        const auto plan = PlanAudioQueueWrite(
            kAudioOutputDesignMaxQueueBytes - 1,
            2048);
        if (plan.framesToWrite != 0) {
            std::cerr << "sub-frame room must not queue a partial frame\n";
            return 5;
        }
    }

    std::cout
        << "Issue #26 queue write planner PASS target_ms="
        << kAudioOutputTargetQueueMilliseconds
        << " hard_cap_ms="
        << kAudioOutputDesignMaxQueueMilliseconds
        << "\n";
    return 0;
}
