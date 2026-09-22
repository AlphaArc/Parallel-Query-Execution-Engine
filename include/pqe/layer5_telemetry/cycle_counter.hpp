#ifndef PQE_TELEMETRY_CYCLE_COUNTER_HPP
#define PQE_TELEMETRY_CYCLE_COUNTER_HPP

#include <cstdint>

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    #include <x86intrin.h>
#endif

namespace pqe::telemetry {

    /**
     * @brief High-precision hardware cycle counter using x86 rdtsc intrinsic.
     *        Provides exact CPU clock cycles elapsed.
     */
    class CycleCounter {
    public:
        CycleCounter() noexcept : start_cycles_(0), end_cycles_(0) {}

        inline void start() noexcept {
            start_cycles_ = __rdtsc();
        }

        inline void stop() noexcept {
            end_cycles_ = __rdtsc();
        }

        [[nodiscard]] inline std::uint64_t elapsed_cycles() const noexcept {
            return end_cycles_ > start_cycles_ ? (end_cycles_ - start_cycles_) : 0;
        }

    private:
        std::uint64_t start_cycles_;
        std::uint64_t end_cycles_;
    };

} // namespace pqe::telemetry

#endif // PQE_TELEMETRY_CYCLE_COUNTER_HPP
