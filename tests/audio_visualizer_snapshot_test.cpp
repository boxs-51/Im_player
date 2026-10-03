#include "player/audio/AudioVisualizerSnapshot.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
void FillFrame(AudioVisualizerFrame& frame, std::uint64_t sequence)
{
    const float marker = static_cast<float>(sequence);

    frame.sequence = sequence;
    frame.pts = static_cast<double>(sequence);
    frame.peakLeft = marker;
    frame.peakRight = marker;
    frame.rmsLeft = marker;
    frame.rmsRight = marker;
    frame.clipCount = static_cast<std::uint32_t>(sequence);
    frame.subBassEnergy = marker;
    frame.bassEnergy = marker;
    frame.midEnergy = marker;
    frame.trebleEnergy = marker;
    frame.spectrum.assign(kSpectrumBins, marker);
}

bool IsSelfConsistent(const AudioVisualizerFrame& frame)
{
    if (frame.sequence == 0)
        return true;

    const float marker = static_cast<float>(frame.sequence);
    return frame.pts == static_cast<double>(frame.sequence)
        && frame.peakLeft == marker
        && frame.peakRight == marker
        && frame.rmsLeft == marker
        && frame.rmsRight == marker
        && frame.clipCount == static_cast<std::uint32_t>(frame.sequence)
        && frame.subBassEnergy == marker
        && frame.bassEnergy == marker
        && frame.midEnergy == marker
        && frame.trebleEnergy == marker
        && frame.spectrum.size() == kSpectrumBins
        && std::all_of(
            frame.spectrum.begin(),
            frame.spectrum.end(),
            [marker](float value) { return value == marker; });
}
} // namespace

int main()
{
    constexpr std::uint64_t kPublishes = 50000;
    constexpr int kReaderCount = 4;

    AudioVisualizerSnapshot snapshot;
    AudioVisualizerFrame producerFrame;

    std::atomic<bool> stop{false};
    std::atomic<bool> violation{false};
    std::atomic<std::uint64_t> reads{0};
    std::vector<std::thread> readers;
    readers.reserve(kReaderCount);

    for (int index = 0; index < kReaderCount; ++index)
    {
        readers.emplace_back([&] {
            AudioVisualizerFrame local;
            while (!stop.load(std::memory_order_acquire))
            {
                snapshot.Read(local);
                if (!IsSelfConsistent(local))
                {
                    violation.store(true, std::memory_order_release);
                    break;
                }
                reads.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }

    for (std::uint64_t sequence = 1; sequence <= kPublishes; ++sequence)
    {
        FillFrame(producerFrame, sequence);
        snapshot.Publish(producerFrame);

        if ((sequence % 64) == 0)
            std::this_thread::yield();

        if (violation.load(std::memory_order_acquire))
            break;
    }

    stop.store(true, std::memory_order_release);
    for (auto& reader : readers)
        reader.join();

    AudioVisualizerFrame finalFrame;
    snapshot.Read(finalFrame);

    if (violation.load(std::memory_order_acquire))
    {
        std::cerr << "visualizer snapshot became torn under concurrent publish/read\n";
        return 1;
    }

    if (finalFrame.sequence != kPublishes || !IsSelfConsistent(finalFrame))
    {
        std::cerr << "final visualizer snapshot is inconsistent: sequence="
                  << finalFrame.sequence << "\n";
        return 2;
    }

    if (reads.load(std::memory_order_relaxed) == 0)
    {
        std::cerr << "stress test did not exercise any concurrent reads\n";
        return 3;
    }

    std::cout << "Issue #20 visualizer snapshot stress PASS: publishes="
              << kPublishes << " reads=" << reads.load(std::memory_order_relaxed)
              << "\n";
    return 0;
}
