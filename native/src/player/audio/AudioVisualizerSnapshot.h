#pragma once

#include "AudioVisualizerData.h"

#include <mutex>
#include <utility>

/**
 * @brief Race-free publication boundary for AudioVisualizerFrame snapshots.
 *
 * The producer owns its working frame outside this object. Publish() swaps that
 * producer-only frame with the guarded published frame under a short mutex.
 * Readers copy only the published frame while holding the same mutex.
 *
 * The mutex intentionally protects snapshot publication/copy only; DSP/FFT
 * analysis remains outside the critical section.
 */
class AudioVisualizerSnapshot final {
public:
    void Publish(AudioVisualizerFrame& producerFrame)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        using std::swap;
        swap(m_publishedFrame, producerFrame);
    }

    void Read(AudioVisualizerFrame& outFrame) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        outFrame = m_publishedFrame;
    }

private:
    mutable std::mutex m_mutex;
    AudioVisualizerFrame m_publishedFrame;
};
