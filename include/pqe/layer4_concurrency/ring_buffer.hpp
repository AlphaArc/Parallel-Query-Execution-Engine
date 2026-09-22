#ifndef PQE_CONCURRENCY_RING_BUFFER_HPP
#define PQE_CONCURRENCY_RING_BUFFER_HPP

#include <atomic>
#include <vector>
#include <optional>
#include <cassert>

namespace pqe::concurrency {

    /**
     * @brief A lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer.
     *        Uses acquire/release memory semantics for extreme zero-copy throughput.
     */
    template <typename T>
    class SPSCRingBuffer {
    public:
        explicit SPSCRingBuffer(std::size_t capacity) 
            : capacity_(capacity + 1), buffer_(capacity + 1) {
            // capacity + 1 is used to distinguish full vs empty state
            head_.store(0, std::memory_order_relaxed);
            tail_.store(0, std::memory_order_relaxed);
        }

        /**
         * @brief Attempts to push an item to the buffer.
         * @return true if successful, false if the buffer is full.
         */
        bool push(const T& item) noexcept {
            const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
            const std::size_t next_tail = (current_tail + 1) % capacity_;

            // We must acquire head to ensure we see the latest consumer reads
            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false; // Full
            }

            buffer_[current_tail] = item;
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }
        
        bool push(T&& item) noexcept {
            const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
            const std::size_t next_tail = (current_tail + 1) % capacity_;

            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false;
            }

            buffer_[current_tail] = std::move(item);
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }

        /**
         * @brief Attempts to pop an item from the buffer.
         * @return std::optional containing the item if successful, std::nullopt if empty.
         */
        std::optional<T> pop() noexcept {
            const std::size_t current_head = head_.load(std::memory_order_relaxed);
            
            // We must acquire tail to ensure we see the latest producer writes
            if (current_head == tail_.load(std::memory_order_acquire)) {
                return std::nullopt; // Empty
            }

            T item = std::move(buffer_[current_head]);
            head_.store((current_head + 1) % capacity_, std::memory_order_release);
            return item;
        }

    private:
        const std::size_t capacity_;
        std::vector<T> buffer_;

        alignas(64) std::atomic<std::size_t> head_;
        alignas(64) std::atomic<std::size_t> tail_;
    };

} // namespace pqe::concurrency

#endif // PQE_CONCURRENCY_RING_BUFFER_HPP
