#pragma once

#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"
#include "pqe/layer3_execution/sequential/seq_aggregator.hpp" // For ScalarAggregateResult

#include <vector>
#include <cstdint>

namespace pqe::execution::parallel {

    using sequential::ScalarAggregateResult;

    class OmpAggregator {
    public:
        explicit OmpAggregator(const storage::ColumnarTable& table) noexcept;

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

} // namespace pqe::execution::parallel
