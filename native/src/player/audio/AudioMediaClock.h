#pragma once

#include "AudioTypes.h"

#include <cstdint>

inline constexpr double CanonicalFramesToSeconds(std::uint64_t frames) noexcept
{
    return static_cast<double>(frames) /
           static_cast<double>(kCanonicalAudioSampleRate);
}

/**
 * @brief Media-time estimate advanced only by canonical PCM frame count.
 *
 * A reference clock (currently MPV time-pos) is sampled exactly once per audio
 * epoch to establish the media anchor. After anchoring, the audio timeline must
 * never be re-anchored from that reference on every block; only canonical PCM
 * frame count advances the media position.
 */
class AudioMediaClock final {
public:
    bool IsAnchored() const noexcept
    {
        return m_anchored;
    }

    void Anchor(double mediaPts) noexcept
    {
        m_anchorPts = mediaPts;
        m_framesSinceAnchor = 0;
        m_anchored = true;
    }

    void Reset() noexcept
    {
        m_anchorPts = 0.0;
        m_framesSinceAnchor = 0;
        m_anchored = false;
    }

    double CurrentPosition() const noexcept
    {
        return m_anchorPts + CanonicalFramesToSeconds(m_framesSinceAnchor);
    }

    void Advance(std::uint32_t frames) noexcept
    {
        m_framesSinceAnchor += frames;
    }

    double AnchorPts() const noexcept
    {
        return m_anchorPts;
    }

    std::uint64_t FramesSinceAnchor() const noexcept
    {
        return m_framesSinceAnchor;
    }

private:
    bool m_anchored = false;
    double m_anchorPts = 0.0;
    std::uint64_t m_framesSinceAnchor = 0;
};
