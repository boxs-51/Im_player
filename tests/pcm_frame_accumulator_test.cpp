#include "player/audio/PcmFrameAccumulator.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
std::vector<std::uint8_t> BytesFromSamples(const std::vector<float>& samples)
{
    std::vector<std::uint8_t> bytes(samples.size() * sizeof(float));
    std::memcpy(bytes.data(), samples.data(), bytes.size());
    return bytes;
}

bool SameFloat(float a, float b)
{
    std::uint32_t left = 0;
    std::uint32_t right = 0;
    std::memcpy(&left, &a, sizeof(left));
    std::memcpy(&right, &b, sizeof(right));
    return left == right;
}
} // namespace

int main()
{
    static_assert(kCanonicalAudioSampleRate == 48000);
    static_assert(kCanonicalAudioChannels == 2);
    static_assert(PcmFrameAccumulator::kFrameBytes == sizeof(float) * 2);

    const std::vector<float> source{
        1.0f, -1.0f,
        0.25f, -0.25f,
        0.5f, -0.5f,
    };
    const auto bytes = BytesFromSamples(source);

    PcmFrameAccumulator accumulator;
    std::array<float, kMaxAudioSamples> output{};

    if (!accumulator.Append(bytes.data(), 3)) {
        std::cerr << "append 3 bytes failed\n";
        return 1;
    }
    if (accumulator.ReadyFrames() != 0 || accumulator.PendingBytes() != 3) {
        std::cerr << "partial frame became visible\n";
        return 2;
    }

    if (!accumulator.Append(bytes.data() + 3, 5)) {
        std::cerr << "append remainder failed\n";
        return 3;
    }
    if (accumulator.ReadyFrames() != 1) {
        std::cerr << "complete stereo frame not detected\n";
        return 4;
    }

    if (accumulator.DrainFrames(output.data(), 1) != 1) {
        std::cerr << "first frame drain failed\n";
        return 5;
    }
    if (!SameFloat(output[0], source[0]) || !SameFloat(output[1], source[1])) {
        std::cerr << "first frame payload changed\n";
        return 6;
    }
    if (accumulator.PendingBytes() != 0) {
        std::cerr << "first frame left unexpected carry\n";
        return 7;
    }

    const size_t secondOffset = PcmFrameAccumulator::kFrameBytes;
    const size_t remainingBytes = bytes.size() - secondOffset;
    if (!accumulator.Append(bytes.data() + secondOffset, 7) ||
        !accumulator.Append(bytes.data() + secondOffset + 7, remainingBytes - 7)) {
        std::cerr << "split multi-frame append failed\n";
        return 8;
    }

    if (accumulator.ReadyFrames() != 2) {
        std::cerr << "expected two complete frames\n";
        return 9;
    }

    if (accumulator.DrainFrames(output.data(), 1) != 1) {
        std::cerr << "bounded drain failed\n";
        return 10;
    }
    if (!SameFloat(output[0], source[2]) || !SameFloat(output[1], source[3])) {
        std::cerr << "second frame payload changed\n";
        return 11;
    }
    if (accumulator.ReadyFrames() != 1) {
        std::cerr << "bounded drain consumed too much\n";
        return 12;
    }

    if (accumulator.DrainFrames(output.data(), static_cast<uint32_t>(kMaxAudioFrames)) != 1) {
        std::cerr << "final drain failed\n";
        return 13;
    }
    if (!SameFloat(output[0], source[4]) || !SameFloat(output[1], source[5])) {
        std::cerr << "final frame payload changed\n";
        return 14;
    }
    if (accumulator.PendingBytes() != 0) {
        std::cerr << "final carry not empty\n";
        return 15;
    }

    std::cout << "Issue #26 PCM frame accumulator PASS\n";
    return 0;
}
