#pragma once

#include "AudioTypes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

/**
 * @brief Byte-stream to complete canonical PCM-frame accumulator.
 *
 * Named pipes are byte streams: ReadFile() boundaries are not PCM frame
 * boundaries. This helper retains incomplete trailing bytes and only exposes
 * complete float32/stereo frames to AudioBlock storage.
 */
class PcmFrameAccumulator final {
public:
    static constexpr size_t kFrameBytes = kCanonicalAudioBytesPerFrame;
    static constexpr size_t kReadCapacity = kMaxAudioSamples * sizeof(float);
    static constexpr size_t kCapacity = kReadCapacity + kFrameBytes - 1;

    bool Append(const void* data, size_t byteCount) noexcept
    {
        if (!data || byteCount == 0)
            return true;
        if (byteCount > (kCapacity - m_size))
            return false;

        std::memcpy(m_storage.data() + m_size, data, byteCount);
        m_size += byteCount;
        return true;
    }

    size_t ReadyFrames() const noexcept
    {
        return m_size / kFrameBytes;
    }

    size_t PendingBytes() const noexcept
    {
        return m_size;
    }

    uint32_t DrainFrames(float* output, uint32_t maxFrames) noexcept
    {
        if (!output || maxFrames == 0)
            return 0;

        const size_t frames =
            std::min<size_t>(ReadyFrames(), static_cast<size_t>(maxFrames));
        const size_t bytes = frames * kFrameBytes;
        if (bytes == 0)
            return 0;

        std::memcpy(output, m_storage.data(), bytes);
        m_size -= bytes;
        if (m_size != 0)
            std::memmove(m_storage.data(), m_storage.data() + bytes, m_size);

        return static_cast<uint32_t>(frames);
    }

    void Reset() noexcept
    {
        m_size = 0;
    }

private:
    std::array<std::uint8_t, kCapacity> m_storage{};
    size_t m_size = 0;
};
