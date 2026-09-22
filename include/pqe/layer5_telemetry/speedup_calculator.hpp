#pragma once

#include "pqe/layer5_telemetry/telemetry.hpp"

#include <string>
#include <iostream>
#include <iomanip>

namespace pqe::telemetry {

    struct ParallelSpeedupMetrics {
        QueryMetrics seq_metrics;
        QueryMetrics par_metrics;
        int threads_used;

        [[nodiscard]] double speedup() const noexcept {
            if (par_metrics.duration_microseconds <= 0.0) return 0.0;
            return seq_metrics.duration_microseconds / par_metrics.duration_microseconds;
        }

        [[nodiscard]] double efficiency() const noexcept {
            if (threads_used <= 0) return 0.0;
            return speedup() / static_cast<double>(threads_used);
        }

        void print_report() const {
            std::cout << "[TELEMETRY - PARALLEL SPEEDUP] " << seq_metrics.query_name << ":\n"
                      << "  -> Threads Used:   " << threads_used << "\n"
                      << "  -> Seq Cycles:     " << seq_metrics.cpu_cycles << "\n"
                      << "  -> Par Cycles:     " << par_metrics.cpu_cycles << "\n"
                      << "  -> Seq Time (T1):  " << std::fixed << std::setprecision(2) << seq_metrics.duration_milliseconds << " ms\n"
                      << "  -> Par Time (TN):  " << std::fixed << std::setprecision(2) << par_metrics.duration_milliseconds << " ms\n"
                      << "  -> Speedup (S_N):  " << std::fixed << std::setprecision(2) << speedup() << "x\n"
                      << "  -> Efficiency (E): " << std::fixed << std::setprecision(2) << (efficiency() * 100.0) << "%\n";
        }
    };

} // namespace pqe::telemetry
