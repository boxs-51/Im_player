// EventQueue.h
#pragma once
#include <SDL_events.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

/**
 * @brief Một hàng đợi sự kiện thread-safe để giao tiếp giữa luồng chính và các luồng render.
 */
class EventQueue {
public:
    EventQueue() = default;

    // Cấm sao chép và gán để tránh các vấn đề về ownership
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;

    void Push(const SDL_Event& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(event);
        m_cv.notify_one();
    }

    bool TryPop(SDL_Event& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        event = m_queue.front();
        m_queue.pop();
        return true;
    }

private:
    std::queue<SDL_Event> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};
class WindowRuntime;
void HandleWindowRuntimeEvent(WindowRuntime *runtime, const SDL_Event *e);