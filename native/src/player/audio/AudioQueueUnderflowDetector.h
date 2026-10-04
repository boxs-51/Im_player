#pragma once

#include "AudioTypes.h"

#include <cstdint>

enum class AudioQueueEmptyEvent {
    None,
    Observed,
    Recovered,
    SustainedUnderflow,
};

struct AudioQueueEmptyDecision {
    AudioQueueEmptyEvent event = AudioQueueEmptyEvent::None;
    std::uint64_t emptyDurationMicros = 0;
};

/**
 * @brief Distinguishes transient SDL queue-empty observations from sustained
 * starvation.
 *
 * SDL queued mode can momentarily report zero bytes around refill boundaries.
 * Counting the first zero poll as an audible underrun is too eager and can
 * cause a self-inflicted gap if recovery immediately flushes/rearms playback.
 * The detector therefore requires a continuous zero interval of at least two
 * requested SDL device periods before declaring a real underflow.
 */
class AudioQueueUnderflowDetector final {
public:
    AudioQueueEmptyDecision Observe(
        bool playbackStarted,
        std::uint32_t queuedBytes,
        std::uint64_t nowMicros) noexcept
    {
        if (!playbackStarted) {
            Reset();
            return {};
        }

        if (queuedBytes > 0) {
            if (!m_zeroActive) {
                return {};
            }

            const std::uint64_t duration =
                nowMicros >= m_zeroSinceMicros
                    ? (nowMicros - m_zeroSinceMicros)
                    : 0;
            Reset();
            return {AudioQueueEmptyEvent::Recovered, duration};
        }

        if (!m_zeroActive) {
            m_zeroActive = true;
            m_zeroSinceMicros = nowMicros;
            return {AudioQueueEmptyEvent::Observed, 0};
        }

        const std::uint64_t duration =
            nowMicros >= m_zeroSinceMicros
                ? (nowMicros - m_zeroSinceMicros)
                : 0;

        if (!m_underflowLatched &&
            duration >= kAudioOutputUnderflowGraceMicros) {
            m_underflowLatched = true;
            return {AudioQueueEmptyEvent::SustainedUnderflow, duration};
        }

        return {};
    }

    void Reset() noexcept
    {
        m_zeroActive = false;
        m_underflowLatched = false;
        m_zeroSinceMicros = 0;
    }

    bool ZeroActive() const noexcept
    {
        return m_zeroActive;
    }

private:
    bool m_zeroActive = false;
    bool m_underflowLatched = false;
    std::uint64_t m_zeroSinceMicros = 0;
};
