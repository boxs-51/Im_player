#pragma once

#include <vector>
#include <atomic>
#include <cstddef>
#include <utility>

/**
 * @class SpscRingBuffer
 * @brief Lock-free Single-Producer Single-Consumer Ring Buffer.
 */
template <typename T>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(size_t capacity)
        : m_capacity(capacity + 1), // Đệm thêm 1 phần tử để phân biệt Full / Empty
          m_buffer(capacity + 1),
          m_head(0),
          m_tail(0) {}

    ~SpscRingBuffer() = default;

    // Cấm copy
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    /**
     * @brief Đẩy một phần tử vào buffer (Chỉ gọi từ Producer thread).
     */
    bool try_push(const T& item) {
        const size_t current_head = m_head.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) % m_capacity;

        if (next_head == m_tail.load(std::memory_order_acquire)) {
            return false; // Buffer Full
        }

        m_buffer[current_head] = item;
        m_head.store(next_head, std::memory_order_release);
        return true;
    }

    /**
     * @brief Đẩy rvalue (move semantics) vào buffer (Chỉ gọi từ Producer thread).
     */
    bool try_push(T&& item) {
        const size_t current_head = m_head.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) % m_capacity;

        if (next_head == m_tail.load(std::memory_order_acquire)) {
            return false; // Buffer Full
        }

        m_buffer[current_head] = std::move(item);
        m_head.store(next_head, std::memory_order_release);
        return true;
    }

    /**
     * @brief Đẩy dữ liệu vào buffer, nếu buffer đầy thì ghi đè phần tử cũ nhất (DROP_OLDEST).
     */
    void push_overwrite(T&& item) {
        if (!try_push(std::move(item))) {
            T dummy;
            try_pop(dummy); // Pop bỏ item cũ nhất
            try_push(std::move(item));
        }
    }

    /**
     * @brief Lấy một phần tử khỏi buffer (Chỉ gọi từ Consumer thread).
     */
    bool try_pop(T& item) {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);

        if (current_tail == m_head.load(std::memory_order_acquire)) {
            return false; // Buffer Empty
        }

        // 1. Move dữ liệu ra biến nhận
        item = std::move(m_buffer[current_tail]);

        // 2. KHÔI PHỤC VÙNG NHỚ TRONG BUFFER VỀ TRẠNG THÁI SẠCH!
        // Giúp cho lần try_push / operator= tiếp theo không đụng vào con trỏ dở dang (Moved-from state)
        m_buffer[current_tail] = T{}; 

        m_tail.store((current_tail + 1) % m_capacity, std::memory_order_release);
        return true;
    }

    /**
     * @brief Reset hoàn toàn Buffer về trạng thái rỗng
     */
    void clear() noexcept {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);
        const size_t current_head = m_head.load(std::memory_order_relaxed);
        
        // Reset sạch các phần tử chưa pop để giải phóng heap memory của vector/buffer bên trong
        size_t idx = current_tail;
        while (idx != current_head) {
            m_buffer[idx] = T{};
            idx = (idx + 1) % m_capacity;
        }

        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_release);
    }

    size_t capacity() const noexcept {
        return m_capacity - 1;
    }

    size_t size() const noexcept {
        const size_t head = m_head.load(std::memory_order_relaxed);
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        if (head >= tail) {
            return head - tail;
        }
        return m_capacity + head - tail;
    }

    bool empty() const noexcept {
        return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
    }

private:
    const size_t m_capacity;
    std::vector<T> m_buffer;

    // Cache line alignment để tránh False Sharing
    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
};