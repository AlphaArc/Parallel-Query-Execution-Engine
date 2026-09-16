#pragma once

#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"

#include <vector>
#include <functional>

namespace pqe::execution::sequential {

    class SeqFilter {
    public:
        explicit SeqFilter(const storage::ColumnarTable& table) noexcept;

        // Custom predicate evaluation over full table
        [[nodiscard]] std::vector<row_id_t> filter(
            const std::function<bool(row_id_t)>& predicate) const;

        // Custom predicate evaluation over a previous selection vector (pipelining)
        [[nodiscard]] std::vector<row_id_t> filter_subset(
            const std::vector<row_id_t>& input_selection,
            const std::function<bool(row_id_t)>& predicate) const;

        // Fast specialized vector-direct filter: Quantity > threshold
        [[nodiscard]] std::vector<row_id_t> filter_quantity_gt(std::int32_t threshold) const;

        // Fast specialized vector-direct filter: CategoryID == target_category
        [[nodiscard]] std::vector<row_id_t> filter_category_eq(std::int32_t target_category) const;

        // Fast specialized conjunction filter: Quantity > threshold AND CategoryID == target_category
        [[nodiscard]] std::vector<row_id_t> filter_quantity_and_category(
            std::int32_t quantity_gt, std::int32_t category_eq) const;

    private:
        const storage::ColumnarTable& table_;
    };

} // namespace pqe::execution::sequential
