#include "player/audio/AudioMediaClock.h"

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
    AudioMediaClock clock;
    if (clock.IsAnchored()) {
        std::cerr << "fresh clock must be unanchored\n";
        return 1;
    }

    clock.Anchor(0.0);
    clock.Advance(kCanonicalAudioSampleRate);
    if (!Near(clock.CurrentPosition(), 1.0)) {
        std::cerr << "one-second frame advance failed\n";
        return 2;
    }

    // Auditor-required synthetic drift proof:
    // audio sample/media clock runs +1000 ppm against a 600 s reference.
    // An independent frame-count clock must accumulate ~+0.600 s drift.
    clock.Reset();
    clock.Anchor(0.0);
    constexpr std::uint64_t referenceSeconds = 600;
    constexpr std::uint64_t ppmNumerator = 1001;
    constexpr std::uint64_t ppmDenominator = 1000;
    constexpr std::uint64_t fastFrames =
        static_cast<std::uint64_t>(kCanonicalAudioSampleRate) *
        referenceSeconds *
        ppmNumerator /
        ppmDenominator;
    clock.Advance(static_cast<std::uint32_t>(fastFrames));

    const double measuredDrift =
        clock.CurrentPosition() - static_cast<double>(referenceSeconds);
    if (!Near(measuredDrift, 0.600, 1e-6)) {
        std::cerr << "synthetic +1000ppm drift did not accumulate: "
                  << measuredDrift << "\n";
        return 3;
    }

    clock.Reset();
    clock.Anchor(42.5);
    clock.Advance(kCanonicalAudioSampleRate / 2);
    if (!Near(clock.CurrentPosition(), 43.0)) {
        std::cerr << "non-zero epoch anchor failed\n";
        return 4;
    }

    std::cout << "Issue #26 independent audio media clock PASS\n";
    return 0;
}
