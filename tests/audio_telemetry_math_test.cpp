#include "player/audio/AudioTelemetry.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace
{
bool Near(double a, double b, double epsilon = 1e-9)
{
    return std::fabs(a - b) <= epsilon;
}
}

int main()
{
    static_assert(kCanonicalAudioSampleRate == 48000);
    static_assert(kCanonicalAudioChannels == 2);
    static_assert(kCanonicalAudioBytesPerFrame == sizeof(float) * 2);

    const std::uint32_t oneSecondQueued =
        kCanonicalAudioSampleRate *
        static_cast<std::uint32_t>(kCanonicalAudioBytesPerFrame);

    if (!Near(CanonicalQueuedAudioSeconds(oneSecondQueued), 1.0)) {
        std::cerr << "queued-duration conversion failed\n";
        return 1;
    }

    const double lastWrittenEndPts = 12.5;
    if (!Near(EstimateAudibleHeadPts(lastWrittenEndPts, oneSecondQueued), 11.5)) {
        std::cerr << "audible-head estimate failed\n";
        return 2;
    }

    if (!Near(
            EstimateAudioVideoOffsetSeconds(
                lastWrittenEndPts,
                oneSecondQueued,
                11.25),
            0.25)) {
        std::cerr << "A/V offset estimate failed\n";
        return 3;
    }

    std::atomic<std::uint64_t> first{0};
    if (!RecordFirstAudioTelemetry(first, 100)) {
        std::cerr << "first timestamp was not accepted\n";
        return 4;
    }
    if (RecordFirstAudioTelemetry(first, 200)) {
        std::cerr << "second timestamp replaced first\n";
        return 5;
    }
    if (first.load(std::memory_order_relaxed) != 100) {
        std::cerr << "first timestamp changed\n";
        return 6;
    }

    std::cout << "Issue #26 audio telemetry math PASS\n";
    return 0;
}
