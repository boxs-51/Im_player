#include "player/render/RenderCallbackLifetimeGate.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
    constexpr int kRounds = 200;
    constexpr int kThreads = 4;

    for (int round = 0; round < kRounds; ++round)
    {
        RenderCallbackLifetimeGate gate;
        std::atomic<bool> stop{false};
        std::atomic<bool> protectedAlive{true};
        std::atomic<bool> violation{false};
        std::atomic<int> readyWorkers{0};
        std::atomic<unsigned long long> acceptedCalls{0};
        std::vector<std::thread> workers;
        workers.reserve(kThreads);

        for (int i = 0; i < kThreads; ++i)
        {
            workers.emplace_back([&] {
                readyWorkers.fetch_add(1, std::memory_order_release);
                while (!stop.load(std::memory_order_acquire))
                {
                    auto invocation = gate.Enter();
                    if (invocation)
                    {
                        if (!protectedAlive.load(std::memory_order_acquire))
                            violation.store(true, std::memory_order_release);

                        acceptedCalls.fetch_add(1, std::memory_order_relaxed);
                        std::this_thread::yield();

                        if (!protectedAlive.load(std::memory_order_acquire))
                            violation.store(true, std::memory_order_release);
                    }
                }
            });
        }

        while (readyWorkers.load(std::memory_order_acquire) != kThreads)
            std::this_thread::yield();

        while (acceptedCalls.load(std::memory_order_acquire) == 0)
            std::this_thread::yield();

        gate.Close();
        gate.WaitForQuiescence();

        // Models destruction of PlayBackRenderThread. Callbacks may continue to
        // arrive, but after Close() none may enter the protected region.
        protectedAlive.store(false, std::memory_order_release);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        stop.store(true, std::memory_order_release);

        for (auto& worker : workers)
            worker.join();

        gate.WaitForQuiescence();

        if (violation.load(std::memory_order_acquire))
        {
            std::cerr << "callback accessed protected lifetime after quiescence in round "
                      << round << "\n";
            return 1;
        }

        if (gate.IsAccepting() || gate.InFlight() != 0)
        {
            std::cerr << "gate did not close cleanly in round " << round << "\n";
            return 2;
        }

        if (acceptedCalls.load(std::memory_order_relaxed) == 0)
        {
            std::cerr << "stress round did not exercise an accepted callback: "
                      << round << "\n";
            return 3;
        }
    }

    // Exercise the exact waiter/last-leaver synchronization repeatedly.
    // The last Invocation release must never be able to notify before a
    // quiescence waiter is safely blocked.
    constexpr int kWaiterRounds = 2000;
    for (int round = 0; round < kWaiterRounds; ++round)
    {
        RenderCallbackLifetimeGate gate;
        auto invocation = gate.Enter();
        if (!invocation)
        {
            std::cerr << "waiter round unexpectedly rejected initial invocation: "
                      << round << "\n";
            return 4;
        }

        gate.Close();

        std::atomic<bool> waiterStarted{false};
        std::atomic<bool> waiterDone{false};
        std::thread waiter([&] {
            waiterStarted.store(true, std::memory_order_release);
            gate.WaitForQuiescence();
            waiterDone.store(true, std::memory_order_release);
        });

        while (!waiterStarted.load(std::memory_order_acquire))
            std::this_thread::yield();

        invocation = {};
        waiter.join();

        if (!waiterDone.load(std::memory_order_acquire) || gate.InFlight() != 0)
        {
            std::cerr << "quiescence waiter failed in round " << round << "\n";
            return 5;
        }
    }

    std::cout << "BRG-3 callback lifetime gate stress PASS: "
              << kRounds << " callback rounds, "
              << kWaiterRounds << " waiter rounds\n";
    return 0;
}
