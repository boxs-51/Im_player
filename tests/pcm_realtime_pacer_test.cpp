#include "player/audio/PcmRealtimePacer.h"

#include <cstdint>
#include <iostream>

int main()
{
    static_assert(PcmRealtimePacer::kLeadFrames == kMaxAudioFrames);
    static_assert(PcmRealtimePacer::FramesToMicros(48000) == 1000000);

    PcmRealtimePacer pacer;
    const std::uint64_t start = 1000000;

    auto first = pacer.BeforePublish(start);
    if (first.delayMicros != 0 || first.rebased) {
        std::cerr << "first block must publish immediately\n";
        return 1;
    }
    pacer.OnPublished(static_cast<std::uint32_t>(kMaxAudioFrames));

    auto second = pacer.BeforePublish(start);
    if (second.delayMicros != 0 || second.rebased) {
        std::cerr << "one-block lead must be allowed\n";
        return 2;
    }
    pacer.OnPublished(static_cast<std::uint32_t>(kMaxAudioFrames));

    auto third = pacer.BeforePublish(start);
    const auto blockMicros =
        PcmRealtimePacer::FramesToMicros(kMaxAudioFrames);
    if (third.delayMicros < blockMicros - 1 ||
        third.delayMicros > blockMicros + 1 ||
        third.rebased) {
        std::cerr << "third block was not paced by one block duration\n";
        return 3;
    }

    // A long pause/cache stall must not trigger catch-up publication.
    const std::uint64_t late =
        start +
        PcmRealtimePacer::FramesToMicros(pacer.PublishedFrames()) +
        PcmRealtimePacer::kRebaseLagMicros +
        100000;
    auto rebased = pacer.BeforePublish(late);
    if (!rebased.rebased || rebased.delayMicros != 0) {
        std::cerr << "long-stall rebase failed\n";
        return 4;
    }

    pacer.OnPublished(static_cast<std::uint32_t>(kMaxAudioFrames));
    auto afterRebase = pacer.BeforePublish(late);
    if (afterRebase.delayMicros != 0) {
        std::cerr << "one-block lead after rebase must be immediate\n";
        return 5;
    }

    pacer.Reset();
    if (pacer.PublishedFrames() != 0) {
        std::cerr << "reset did not clear published frame count\n";
        return 6;
    }

    std::cout << "Issue #26 PCM realtime pacer PASS\n";
    return 0;
}
