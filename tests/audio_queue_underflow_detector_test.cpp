#include "player/audio/AudioQueueUnderflowDetector.h"

#include <cstdint>
#include <iostream>

int main()
{
    AudioQueueUnderflowDetector detector;

    // Queue zero before playback starts is not an underrun.
    auto event = detector.Observe(false, 0, 1000);
    if (event.event != AudioQueueEmptyEvent::None) {
        std::cerr << "pre-start zero queue must be ignored\n";
        return 1;
    }

    // First zero poll after playback is only an observation.
    event = detector.Observe(true, 0, 2000);
    if (event.event != AudioQueueEmptyEvent::Observed) {
        std::cerr << "first zero queue poll must be Observed\n";
        return 2;
    }

    event = detector.Observe(
        true,
        0,
        2000 + kAudioOutputUnderflowGraceMicros - 1);
    if (event.event != AudioQueueEmptyEvent::None) {
        std::cerr << "transient zero before grace must not underflow\n";
        return 3;
    }

    // Refill before the grace expires is a transient recovery.
    event = detector.Observe(
        true,
        4096,
        2000 + kAudioOutputUnderflowGraceMicros - 1);
    if (event.event != AudioQueueEmptyEvent::Recovered) {
        std::cerr << "refill before grace must recover\n";
        return 4;
    }

    // A fresh continuous zero interval at/after the grace is a real underflow.
    const std::uint64_t secondStart = 100000;
    event = detector.Observe(true, 0, secondStart);
    if (event.event != AudioQueueEmptyEvent::Observed) {
        std::cerr << "second zero interval did not arm\n";
        return 5;
    }

    event = detector.Observe(
        true,
        0,
        secondStart + kAudioOutputUnderflowGraceMicros);
    if (event.event != AudioQueueEmptyEvent::SustainedUnderflow ||
        event.emptyDurationMicros < kAudioOutputUnderflowGraceMicros) {
        std::cerr << "sustained zero was not classified as underflow\n";
        return 6;
    }

    // Latch prevents repeated counting until reset/refill.
    event = detector.Observe(
        true,
        0,
        secondStart + kAudioOutputUnderflowGraceMicros + 5000);
    if (event.event != AudioQueueEmptyEvent::None) {
        std::cerr << "sustained underflow must latch\n";
        return 7;
    }

    detector.Reset();
    if (detector.ZeroActive()) {
        std::cerr << "reset did not clear zero state\n";
        return 8;
    }

    std::cout
        << "Issue #26 queue underflow detector PASS grace_us="
        << kAudioOutputUnderflowGraceMicros
        << "\n";
    return 0;
}
