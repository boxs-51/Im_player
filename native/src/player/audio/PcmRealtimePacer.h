#pragma once

#include "AudioTypes.h"

#include <cstdint>

struct PcmPacingDecision {
    std::uint64_t delayMicros = 0;
    bool rebased = false;
};

/**
 * @brief Real-time pacing contract for the external mpv PCM writer.
 *
 * mpv's PCM AO is a file/pipe writer, not the physical speaker clock. The
 * capture side therefore behaves like a timed sink: it may lead real time by
 * at most one maximum AudioBlock, while long stalls rebase the schedule so a
 * pause/cache stall cannot cause a catch-up burst on resume.
 */
class PcmRealtimePacer final {
public:
    static constexpr std::uint64_t kLeadFrames = kMaxAudioFrames;
    static constexpr std::uint64_t kRebaseLagMicros = 250000;

    static constexpr std::uint64_t FramesToMicros(std::uint64_t frames) noexcept
    {
        return (frames * 1000000ULL + kCanonicalAudioSampleRate - 1ULL) /
               kCanonicalAudioSampleRate;
    }

    PcmPacingDecision BeforePublish(std::uint64_t nowMicros) noexcept
    {
        if (!m_initialized) {
            m_initialized = true;
            m_anchorMicros = nowMicros;
            return {};
        }

        const std::uint64_t publishedDurationMicros =
            FramesToMicros(m_publishedFrames);
        const std::uint64_t expectedMicros =
            m_anchorMicros + publishedDurationMicros;

        if (nowMicros > expectedMicros + kRebaseLagMicros) {
            m_anchorMicros =
                (nowMicros > publishedDurationMicros)
                    ? (nowMicros - publishedDurationMicros)
                    : 0;
            return {0, true};
        }

        const std::uint64_t leadMicros = FramesToMicros(kLeadFrames);
        const std::uint64_t earliestMicros =
            (expectedMicros > leadMicros)
                ? (expectedMicros - leadMicros)
                : m_anchorMicros;

        return {
            nowMicros < earliestMicros ? (earliestMicros - nowMicros) : 0,
            false
        };
    }

    void OnPublished(std::uint32_t frames) noexcept
    {
        m_publishedFrames += frames;
    }

    void Reset() noexcept
    {
        m_initialized = false;
        m_anchorMicros = 0;
        m_publishedFrames = 0;
    }

    std::uint64_t PublishedFrames() const noexcept
    {
        return m_publishedFrames;
    }

private:
    bool m_initialized = false;
    std::uint64_t m_anchorMicros = 0;
    std::uint64_t m_publishedFrames = 0;
};
