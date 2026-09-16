#pragma once

#include <chrono>
#include <string>
#include <iostream>
#include <iomanip>
#include <cstdint>

namespace pqe::telemetry {

    struct QueryMetrics {
        std::string query_name;
        double duration_microseconds{0.0};
        double duration_milliseconds{0.0};
        std::size_t rows_processed{0};
        std::size_t rows_matched{0};
        double throughput_rows_per_sec{0.0};

        void print_report() const {
            std::cout << "[TELEMETRY - T1] " << query_name << ":\n"
                      << "  -> Latency: " << std::fixed << std::setprecision(2) 
                      << duration_milliseconds << " ms (" 
                      << std::fixed << std::setprecision(0) << duration_microseconds << " us)\n"
                      << "  -> Rows Processed: " << rows_processed << "\n"
                      << "  -> Rows Matched: " << rows_matched << "\n"
                      << "  -> Sequential Throughput: " << std::fixed << std::setprecision(0) 
                      << throughput_rows_per_sec << " rows/sec\n";
        }
    };

    class ScopedTimer {
    public:
        using Clock = std::chrono::high_resolution_clock;

        explicit ScopedTimer(std::string name, std::size_t rows_processed = 0)
            : name_(std::move(name)), rows_processed_(rows_processed), start_(Clock::now()) {}

        QueryMetrics stop(std::size_t rows_matched = 0) {
            auto end = Clock::now();
            double us = std::chrono::duration<double, std::micro>(end - start_).count();
            double ms = us / 1000.0;
            double sec = us / 1'000'000.0;
            double throughput = (sec > 0.0) ? (static_cast<double>(rows_processed_) / sec) : 0.0;

            metrics_ = QueryMetrics{
                .query_name = name_,
                .duration_microseconds = us,
                .duration_milliseconds = ms,
                .rows_processed = rows_processed_,
                .rows_matched = rows_matched,
                .throughput_rows_per_sec = throughput
            };
            return metrics_;
        }

        [[nodiscard]] const QueryMetrics& metrics() const noexcept { return metrics_; }

    private:
        std::string name_;
        std::size_t rows_processed_{0};
        Clock::time_point start_;
        QueryMetrics metrics_{};
    };

} // namespace pqe::telemetry
