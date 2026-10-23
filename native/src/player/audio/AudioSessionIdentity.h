#pragma once

// Issue #42 diagnostic IDs, NOT cryptographic provenance or attestation.
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstdint>

inline std::uint64_t AudioTelemetryProcessRunUid() noexcept {
    static const std::uint64_t runUid = []() noexcept {
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto pid = static_cast<std::uint64_t>(GetCurrentProcessId());
        const auto value = static_cast<std::uint64_t>(ticks) ^ (pid << 32);
        return value == 0 ? std::uint64_t{1} : value;
    }();
    return runUid;
}

inline std::uint64_t NextAudioTelemetrySessionUid() noexcept {
    // Zero denotes UNBOUND; unique per process until counter wraps.
    static std::atomic<std::uint64_t> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}
