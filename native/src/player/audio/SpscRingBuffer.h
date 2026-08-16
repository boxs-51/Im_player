#pragma once

#include <vector>
#include <atomic>
#include <cstddef>
#include <utility>
#include <memory>
#include <cstdint>
#include <cassert>

template <typename T> class SpscProducer;
template <typename T> class SpscConsumer;

/**
 * @class SpscRingBufferCore
 * @brief Quản lý bộ nhớ và trạng thái nguyên tử dùng chung.
 */
template <typename T>
class SpscRingBufferCore {
    friend class SpscProducer<T>;
    friend class SpscConsumer<T>;

public:
    explicit SpscRingBufferCore(size_t capacity)
        : m_capacity(capacity + 1),
          m_buffer(capacity + 1),
          m_head(0),
          m_tail(0) {}

    size_t capacity() const noexcept { return m_capacity - 1; }

private:
    const size_t m_capacity;
    std::vector<T> m_buffer;

    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
};

/**
 * @class SpscProducer
 */
template <typename T>
class SpscProducer {
public:
    explicit SpscProducer(std::shared_ptr<SpscRingBufferCore<T>> core)
        : m_core(std::move(core)) {}

    // =========================================================================
    // DEBUG & RAII HANDLE FOR WRITE
    // =========================================================================
    class WriteHandle {
    public:
        WriteHandle(SpscProducer& producer, T* ptr) noexcept
            : m_producer(producer), m_ptr(ptr), m_committed(false) {}

        ~WriteHandle() {
            // Cảnh báo nếu thoát scope mà chưa commit/rollback
            assert(m_committed && "LỖI DEBUG: Đã xin slot ghi (acquire_write) nhưng chưa gọi commit()!");
        }

        // Cấm Copy, chỉ cho Move
        WriteHandle(const WriteHandle&) = delete;
        WriteHandle& operator=(const WriteHandle&) = delete;
        WriteHandle(WriteHandle&& other) noexcept
            : m_producer(other.m_producer), m_ptr(other.m_ptr), m_committed(other.m_committed) {
            other.m_ptr = nullptr;
            other.m_committed = true; // Tránh trigger assert ở đối tượng cũ
        }

        T* get() noexcept { return m_ptr; }
        T* operator->() noexcept { return m_ptr; }
        T& operator*() noexcept { return *m_ptr; }
        explicit operator bool() const noexcept { return m_ptr != nullptr; }

        /// Xác nhận ghi dữ liệu thành công
        void commit() noexcept {
            if (m_ptr && !m_committed) {
                m_producer.commit_write();
                m_committed = true;
            }
        }

        /// Hủy bỏ tác vụ ghi (không commit slot)
        void rollback() noexcept {
            m_committed = true; // Đánh dấu đã xử lý an toàn
        }

    private:
        SpscProducer& m_producer;
        T* m_ptr;
        bool m_committed;
    };

    /// Hàm hỗ trợ RAII safe write
    WriteHandle scoped_acquire_write() noexcept {
        T* ptr = acquire_write();
        return WriteHandle(*this, ptr);
    }

    // =========================================================================
    // API CƠ BẢN
    // =========================================================================
    bool try_push(const T& item) {
        #ifndef NDEBUG
        assert(!m_is_writing && "LỖI DEBUG: Đang trong quá trình Zero-Copy Write!");
        #endif
        const size_t current_head = m_core->m_head.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) % m_core->m_capacity;

        if (next_head == m_core->m_tail.load(std::memory_order_acquire)) return false;

        m_core->m_buffer[current_head] = item;
        m_core->m_head.store(next_head, std::memory_order_release);
        return true;
    }

    bool try_push(T&& item) {
        #ifndef NDEBUG
        assert(!m_is_writing && "LỖI DEBUG: Đang trong quá trình Zero-Copy Write!");
        #endif
        const size_t current_head = m_core->m_head.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) % m_core->m_capacity;

        if (next_head == m_core->m_tail.load(std::memory_order_acquire)) return false;

        m_core->m_buffer[current_head] = std::move(item);
        m_core->m_head.store(next_head, std::memory_order_release);
        return true;
    }

    T* acquire_write() noexcept {
        #ifndef NDEBUG
        assert(!m_is_writing && "LỖI DEBUG: Đã gọi acquire_write() trước đó mà chưa commit_write()!");
        #endif

        const size_t current_head = m_core->m_head.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) % m_core->m_capacity;

        if (next_head == m_core->m_tail.load(std::memory_order_acquire)) return nullptr;

        #ifndef NDEBUG
        m_is_writing = true;
        #endif
        return &m_core->m_buffer[current_head];
    }

    void commit_write() noexcept {
        #ifndef NDEBUG
        assert(m_is_writing && "LỖI DEBUG: Gọi commit_write() nhưng chưa từng gọi acquire_write()!");
        m_is_writing = false;
        #endif

        const size_t current_head = m_core->m_head.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) % m_core->m_capacity;
        m_core->m_head.store(next_head, std::memory_order_release);
    }

private:
    std::shared_ptr<SpscRingBufferCore<T>> m_core;
    #ifndef NDEBUG
    bool m_is_writing{false};
    #endif
};

/**
 * @class SpscConsumer
 */
template <typename T>
class SpscConsumer {
public:
    explicit SpscConsumer(std::shared_ptr<SpscRingBufferCore<T>> core)
        : m_core(std::move(core)) {}

    // =========================================================================
    // DEBUG & RAII HANDLE FOR READ
    // =========================================================================
    class ReadHandle {
    public:
        ReadHandle(SpscConsumer& consumer, const T* ptr) noexcept
            : m_consumer(consumer), m_ptr(ptr), m_released(false) {}

        ~ReadHandle() {
            // Cảnh báo nếu đọc xong mà quên release
            assert(m_released && "LỖI DEBUG: Đã xin slot đọc (acquire_read) nhưng chưa gọi release()!");
        }

        ReadHandle(const ReadHandle&) = delete;
        ReadHandle& operator=(const ReadHandle&) = delete;
        ReadHandle(ReadHandle&& other) noexcept
            : m_consumer(other.m_consumer), m_ptr(other.m_ptr), m_released(other.m_released) {
            other.m_ptr = nullptr;
            other.m_released = true;
        }

        const T* get() const noexcept { return m_ptr; }
        const T* operator->() const noexcept { return m_ptr; }
        const T& operator*() const noexcept { return *m_ptr; }
        explicit operator bool() const noexcept { return m_ptr != nullptr; }

        /// Giải phóng slot sau khi hoàn tất đọc
        void release() noexcept {
            if (m_ptr && !m_released) {
                m_consumer.release_read();
                m_released = true;
            }
        }

    private:
        SpscConsumer& m_consumer;
        const T* m_ptr;
        bool m_released;
    };

    /// Hàm hỗ trợ RAII safe read
    ReadHandle scoped_acquire_read() noexcept {
        const T* ptr = acquire_read();
        return ReadHandle(*this, ptr);
    }

    // =========================================================================
    // API CƠ BẢN
    // =========================================================================
    bool try_pop(T& item) {
        #ifndef NDEBUG
        assert(!m_is_reading && "LỖI DEBUG: Đang trong quá trình Zero-Copy Read!");
        #endif

        const size_t current_tail = m_core->m_tail.load(std::memory_order_relaxed);
        if (current_tail == m_core->m_head.load(std::memory_order_acquire)) return false;

        item = std::move(m_core->m_buffer[current_tail]);
        m_core->m_tail.store((current_tail + 1) % m_core->m_capacity, std::memory_order_release);
        return true;
    }

    const T* acquire_read() const noexcept {
        #ifndef NDEBUG
        assert(!m_is_reading && "LỖI DEBUG: Đã gọi acquire_read() trước đó mà chưa release_read()!");
        #endif

        const size_t current_tail = m_core->m_tail.load(std::memory_order_relaxed);
        if (current_tail == m_core->m_head.load(std::memory_order_acquire)) return nullptr;

        #ifndef NDEBUG
        m_is_reading = true;
        #endif
        return &m_core->m_buffer[current_tail];
    }

    void release_read() noexcept {
        #ifndef NDEBUG
        assert(m_is_reading && "LỖI DEBUG: Gọi release_read() nhưng chưa từng gọi acquire_read()!");
        m_is_reading = false;
        #endif

        const size_t current_tail = m_core->m_tail.load(std::memory_order_relaxed);
        m_core->m_tail.store((current_tail + 1) % m_core->m_capacity, std::memory_order_release);
    }

    uint64_t peek_generation() const noexcept {
        const size_t current_tail = m_core->m_tail.load(std::memory_order_relaxed);
        if (current_tail == m_core->m_head.load(std::memory_order_acquire)) return 0;
        return m_core->m_buffer[current_tail].generation;
    }

    size_t size() const noexcept {
        const size_t head = m_core->m_head.load(std::memory_order_relaxed);
        const size_t tail = m_core->m_tail.load(std::memory_order_relaxed);
        return (head >= tail) ? (head - tail) : (m_core->m_capacity + head - tail);
    }

    bool empty() const noexcept {
        return m_core->m_head.load(std::memory_order_relaxed) == m_core->m_tail.load(std::memory_order_relaxed);
    }

    size_t capacity() const noexcept { return m_core->capacity(); }

private:
    std::shared_ptr<SpscRingBufferCore<T>> m_core;
    #ifndef NDEBUG
    mutable bool m_is_reading{false};
    #endif
};

template <typename T>
std::pair<SpscProducer<T>, SpscConsumer<T>> make_spsc_ring_buffer(size_t capacity) {
    auto core = std::make_shared<SpscRingBufferCore<T>>(capacity);
    return { SpscProducer<T>(core), SpscConsumer<T>(core) };
}