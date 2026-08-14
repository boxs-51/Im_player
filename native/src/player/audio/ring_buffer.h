#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeRingBuffer {
public:
    explicit ThreadSafeRingBuffer(size_t capacity)
        : m_buffer(capacity), m_capacity(capacity), m_head(0), m_tail(0), m_size(0) {}

    // Push an toàn (chờ nếu buffer đầy)
    void push(const T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_not_full.wait(lock, [this] { return !is_full_unsafe(); });
        push_unsafe(item);
        lock.unlock();
        m_cond_not_empty.notify_one();
    }

    // Try Push (không bị từ chối nếu Mutex bận, chỉ từ chối khi Buffer ĐẦY)
    bool try_push(const T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (is_full_unsafe()) {
            return false;
        }
        push_unsafe(item);
        lock.unlock();
        m_cond_not_empty.notify_one();
        return true;
    }

    // Pop an toàn
    bool try_pop(T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (is_empty_unsafe()) {
            return false;
        }
        pop_unsafe(item);
        lock.unlock();
        m_cond_not_full.notify_one();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_head = 0;
        m_tail = 0;
        m_size = 0;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size;
    }

private:
    void push_unsafe(const T& item) {
        m_buffer[m_head] = item;
        m_head = (m_head + 1) % m_capacity;
        m_size++;
    }

    void pop_unsafe(T& item) {
        item = m_buffer[m_tail];
        m_tail = (m_tail + 1) % m_capacity;
        m_size--;
    }

    bool is_full_unsafe() const { return m_size == m_capacity; }
    bool is_empty_unsafe() const { return m_size == 0; }

    std::vector<T> m_buffer;
    const size_t m_capacity;
    size_t m_head;
    size_t m_tail;
    size_t m_size;

    mutable std::mutex m_mutex;
    std::condition_variable m_cond_not_full;
    std::condition_variable m_cond_not_empty;
};