#ifndef PQE_CONCURRENCY_SPIN_LOCK_HPP
#define PQE_CONCURRENCY_SPIN_LOCK_HPP

#include <atomic>

#if defined(_MSC_VER)
    #include <intrin.h>
    #define CPU_PAUSE() _mm_pause()
#elif defined(__GNUC__) || defined(__clang__)
    #include <x86intrin.h>
    #define CPU_PAUSE() _mm_pause()
#else
    #define CPU_PAUSE()
#endif

namespace pqe::concurrency {

    /**
     * @brief An ultra-low latency user-space spinlock utilizing atomic test-and-set.
     *        Designed to avoid OS context switches in highly contended micro-critical sections.
     */
    class SpinLock {
    public:
        inline void lock() noexcept {
            // Spin until the flag is cleared.
            while (flag_.test_and_set(std::memory_order_acquire)) {
                // Yield the CPU execution unit slightly to prevent burning core pipelines
                CPU_PAUSE();
            }
        }

        inline bool try_lock() noexcept {
            return !flag_.test_and_set(std::memory_order_acquire);
        }

        inline void unlock() noexcept {
            flag_.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    };

    /**
     * @brief RAII wrapper for SpinLock (equivalent to std::lock_guard)
     */
    class ScopedSpinLock {
    public:
        explicit ScopedSpinLock(SpinLock& lock) noexcept : lock_(lock) {
            lock_.lock();
        }
        
        ~ScopedSpinLock() {
            lock_.unlock();
        }
        
        // Prevent copying
        ScopedSpinLock(const ScopedSpinLock&) = delete;
        ScopedSpinLock& operator=(const ScopedSpinLock&) = delete;

    private:
        SpinLock& lock_;
    };

} // namespace pqe::concurrency

#endif // PQE_CONCURRENCY_SPIN_LOCK_HPP
