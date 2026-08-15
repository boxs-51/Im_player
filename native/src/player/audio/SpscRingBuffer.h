#pragma once

#include <vector>
#include <atomic>
#include <cstddef>
#include <utility>

/**
 * @class SpscRingBuffer
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer.
 * 
 * QUY TẮC SỬ DỤNG BẮT BUỘC (INVARIANTS):
 * 1. Chỉ duy nhất ONE Producer Thread được gọi: try_push(), acquire_write(), commit_write().
 * 2. Chỉ duy nhất ONE Consumer Thread được gọi: try_pop(), acquire_read(), release_read().
 * 3. Không cho phép gọi clear() bất đồng bộ từ UI Thread hoặc Thread thứ 3 bất kỳ.
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

    // Cấm copy & assign
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    // =========================================================================
    // PRODUCER API (Chỉ được gọi từ Producer Thread)
    // =========================================================================

    /**
     * @brief Đẩy một phần tử lvalue vào buffer (Copy semantics).
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
     * @brief Đẩy một phần tử rvalue vào buffer (Move semantics).
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
     * @brief [ZERO-COPY API] Xin cấp con trỏ slot trống để Producer ghi trực tiếp dữ liệu.
     * @return Con trỏ tới slot trống, hoặc nullptr nếu buffer đã đầy.
     */
    T* acquire_write() noexcept {
        const size_t current_head = m_head.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) % m_capacity;

        if (next_head == m_tail.load(std::memory_order_acquire)) {
            return nullptr; // Buffer Full
        }

        return &m_buffer[current_head];
    }

    /**
     * @brief [ZERO-COPY API] Xác nhận đã ghi xong dữ liệu vào slot từ acquire_write().
     */
    void commit_write() noexcept {
        const size_t current_head = m_head.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) % m_capacity;
        m_head.store(next_head, std::memory_order_release);
    }

    // =========================================================================
    // CONSUMER API (Chỉ được gọi từ Consumer Thread)
    // =========================================================================

    /**
     * @brief Lấy một phần tử khỏi buffer (Move semantics).
     */
    bool try_pop(T& item) {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);

        if (current_tail == m_head.load(std::memory_order_acquire)) {
            return false; // Buffer Empty
        }

        item = std::move(m_buffer[current_tail]);

        // Đã loại bỏ m_buffer[current_tail] = T{} để tối ưu băng thông RAM/CPU.
        m_tail.store((current_tail + 1) % m_capacity, std::memory_order_release);
        return true;
    }

    /**
     * @brief [ZERO-COPY API] Lấy con trỏ hằng tới slot dữ liệu khả dụng nhất để đọc trực tiếp.
     * @return Con trỏ const tới slot, hoặc nullptr nếu buffer rỗng.
     */
    const T* acquire_read() const noexcept {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);

        if (current_tail == m_head.load(std::memory_order_acquire)) {
            return nullptr; // Buffer Empty
        }

        return &m_buffer[current_tail];
    }

    /**
     * @brief [ZERO-COPY API] Giải phóng slot đã đọc xong từ acquire_read().
     */
    void release_read() noexcept {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);
        m_tail.store((current_tail + 1) % m_capacity, std::memory_order_release);
    }

    // =========================================================================
    // METRICS & CAPACITY (Có thể gọi từ bất kỳ thread nào)
    // =========================================================================

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

    size_t occupancy() const noexcept {
        return size();
    }
    /**
     * @brief Đọc giá trị generation của block tiếp theo trong buffer mà KHÔNG release.
     * Trả về 0 nếu buffer rỗng.
     */
    uint64_t peek_generation() const noexcept {
        const size_t current_tail = m_tail.load(std::memory_order_relaxed);
        if (current_tail == m_head.load(std::memory_order_acquire)) {
            return 0; // Buffer empty
        }
        return m_buffer[current_tail].generation;
}

private:
    const size_t m_capacity;
    std::vector<T> m_buffer;

    // Tránh hiện tượng False Sharing giữa Producer (head) và Consumer (tail)
    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
};