#pragma once

#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"

#include <vector>
#include <limits>
#include <span>
#include <iostream>
#include <iomanip>

namespace pqe::execution::sequential {

    struct ScalarAggregateResult {
        std::size_t count{0};
        double sum{0.0};
        double avg{0.0};
        double min{std::numeric_limits<double>::infinity()};
        double max{-std::numeric_limits<double>::infinity()};

        void print_report(std::string_view metric_name) const {
            std::cout << "  [" << metric_name << " Aggregation]\n"
                      << "    Count: " << count << "\n"
                      << "    Sum:   " << std::fixed << std::setprecision(2) << sum << "\n"
                      << "    Avg:   " << std::fixed << std::setprecision(2) << avg << "\n"
                      << "    Min:   " << std::fixed << std::setprecision(2) << min << "\n"
                      << "    Max:   " << std::fixed << std::setprecision(2) << max << "\n";
        }
    };

    class SeqAggregator {
    public:
        explicit SeqAggregator(const storage::ColumnarTable& table) noexcept;

        // COUNT: over full table or selection vector
        [[nodiscard]] std::size_t count() const noexcept;
        [[nodiscard]] std::size_t count(const std::vector<row_id_t>& selection) const noexcept;

        // SUM on Price column
        [[nodiscard]] double sum_price() const noexcept;
        [[nodiscard]] double sum_price(const std::vector<row_id_t>& selection) const noexcept;

        // SUM on Quantity column
        [[nodiscard]] std::int64_t sum_quantity() const noexcept;
        [[nodiscard]] std::int64_t sum_quantity(const std::vector<row_id_t>& selection) const noexcept;

        // Comprehensive single-pass Price aggregation: COUNT, SUM, AVG, MIN, MAX
        [[nodiscard]] ScalarAggregateResult aggregate_price() const noexcept;
        [[nodiscard]] ScalarAggregateResult aggregate_price(const std::vector<row_id_t>& selection) const noexcept;

        // Net Sales Amount aggregation: SUM(Price * (1.0 - Discount) * Quantity)
        [[nodiscard]] double sum_net_sales() const noexcept;
        [[nodiscard]] double sum_net_sales(const std::vector<row_id_t>& selection) const noexcept;

    private:
        const storage::ColumnarTable& table_;
    };

} // namespace pqe::execution::sequential
