#ifndef PQE_CONCURRENCY_ATOMIC_ACCUMULATOR_HPP
#define PQE_CONCURRENCY_ATOMIC_ACCUMULATOR_HPP

#include <atomic>
#include <type_traits>

namespace pqe::concurrency {

    /**
     * @brief Lock-free accumulator wrapper for metrics.
     *        Provides true lock-free Compare-And-Swap (CAS) addition for floating-point types,
     *        and uses standard fetch_add for integral types.
     */
    template <typename T>
    class AtomicAccumulator {
    public:
        AtomicAccumulator(T initial = T(0)) noexcept : value_(initial) {}

        inline void add(T val) noexcept {
            if constexpr (std::is_integral_v<T>) {
                value_.fetch_add(val, std::memory_order_relaxed);
            } else {
                // Lock-free floating point addition via CAS loop
                T current = value_.load(std::memory_order_relaxed);
                while (!value_.compare_exchange_weak(current, current + val,
                                                     std::memory_order_relaxed,
                                                     std::memory_order_relaxed)) {
                    // loop automatically reloads 'current' on failure
                }
            }
        }

        inline void update_max(T val) noexcept {
            T current = value_.load(std::memory_order_relaxed);
            while (val > current && !value_.compare_exchange_weak(current, val,
                                                                  std::memory_order_relaxed,
                                                                  std::memory_order_relaxed)) {
            }
        }

        inline void update_min(T val) noexcept {
            T current = value_.load(std::memory_order_relaxed);
            while (val < current && !value_.compare_exchange_weak(current, val,
                                                                  std::memory_order_relaxed,
                                                                  std::memory_order_relaxed)) {
            }
        }

        [[nodiscard]] inline T load() const noexcept {
            return value_.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<T> value_;
    };

} // namespace pqe::concurrency

#endif // PQE_CONCURRENCY_ATOMIC_ACCUMULATOR_HPP
