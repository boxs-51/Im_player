#pragma once

#include <vector>
#include <atomic>
#include <optional>

#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeRingBuffer {
public:
    // Khởi tạo buffer với một dung lượng cụ thể.
    explicit ThreadSafeRingBuffer(size_t capacity)
        : m_buffer(capacity), m_capacity(capacity), m_head(0), m_tail(0), m_size(0) {}

    // Đẩy một phần tử vào buffer (non-blocking).
    // Trả về `true` nếu thành công, `false` nếu buffer đầy.
    bool try_push(const T& item) {
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock() || is_full_unsafe()) {
            return false;
        }
        push_unsafe(item);
        lock.unlock();
        m_cond_not_empty.notify_one();
        return true;
    }

    // Lấy một phần tử ra khỏi buffer (non-blocking).
    // Trả về `true` nếu thành công, `false` nếu buffer rỗng.
    bool try_pop(T& item) {
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock() || is_empty_unsafe()) {
            return false;
        }
        pop_unsafe(item);
        lock.unlock();
        m_cond_not_full.notify_one();
        return true;
    }

    // Đẩy một phần tử vào buffer (blocking).
    // Sẽ chờ nếu buffer đầy.
    void push(const T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_not_full.wait(lock, [this] { return !is_full_unsafe(); });
        push_unsafe(item);
        lock.unlock();
        m_cond_not_empty.notify_one();
    }

    // Lấy một phần tử ra khỏi buffer (blocking).
    // Sẽ chờ nếu buffer rỗng.
    void pop(T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_not_empty.wait(lock, [this] { return !is_empty_unsafe(); });
        pop_unsafe(item);
        lock.unlock();
        m_cond_not_full.notify_one();
    }

    // Kiểm tra xem buffer có rỗng không.
    bool is_empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return is_empty_unsafe();
    }

    // Lấy số lượng phần tử hiện có trong buffer.
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
