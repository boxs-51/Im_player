#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>

class RenderCallbackLifetimeGate {
public:
    class Invocation {
    public:
        Invocation() = default;
        Invocation(const Invocation&) = delete;
        Invocation& operator=(const Invocation&) = delete;

        Invocation(Invocation&& other) noexcept
            : m_owner(other.m_owner), m_accepted(other.m_accepted) {
            other.m_owner = nullptr;
            other.m_accepted = false;
        }

        Invocation& operator=(Invocation&& other) noexcept {
            if (this != &other) {
                Release();
                m_owner = other.m_owner;
                m_accepted = other.m_accepted;
                other.m_owner = nullptr;
                other.m_accepted = false;
            }
            return *this;
        }

        ~Invocation() {
            Release();
        }

        explicit operator bool() const noexcept {
            return m_accepted;
        }

    private:
        friend class RenderCallbackLifetimeGate;

        Invocation(RenderCallbackLifetimeGate* owner, bool accepted) noexcept
            : m_owner(owner), m_accepted(accepted) {}

        void Release() noexcept {
            if (m_owner) {
                m_owner->Leave();
                m_owner = nullptr;
                m_accepted = false;
            }
        }

        RenderCallbackLifetimeGate* m_owner = nullptr;
        bool m_accepted = false;
    };

    Invocation Enter() noexcept {
        m_inFlight.fetch_add(1, std::memory_order_acq_rel);
        const bool accepted = m_accepting.load(std::memory_order_acquire);
        return Invocation(this, accepted);
    }

    void Close() noexcept {
        m_accepting.store(false, std::memory_order_release);
    }

    void WaitForQuiescence() {
        std::unique_lock<std::mutex> lock(m_waitMutex);
        m_waitCv.wait(lock, [this] {
            return m_inFlight.load(std::memory_order_acquire) == 0;
        });
    }

    bool IsAccepting() const noexcept {
        return m_accepting.load(std::memory_order_acquire);
    }

    std::size_t InFlight() const noexcept {
        return m_inFlight.load(std::memory_order_acquire);
    }

private:
    void Leave() noexcept {
        const std::size_t previous = m_inFlight.fetch_sub(1, std::memory_order_acq_rel);
        if (previous == 1) {
            // Synchronize the 1 -> 0 transition with WaitForQuiescence().
            // Without taking the wait mutex here, a waiter can observe a
            // non-zero predicate, then miss the zero-transition notification
            // before it actually blocks on the condition variable.
            std::lock_guard<std::mutex> lock(m_waitMutex);
            m_waitCv.notify_all();
        }
    }

    std::atomic<bool> m_accepting{true};
    std::atomic<std::size_t> m_inFlight{0};
    std::mutex m_waitMutex;
    std::condition_variable m_waitCv;
};
