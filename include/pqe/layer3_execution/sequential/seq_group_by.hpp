#pragma once

#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"

#include <unordered_map>
#include <vector>
#include <limits>
#include <cstdint>

namespace pqe::execution::sequential {

    struct GroupMetrics {
        std::size_t count{0};
        std::int64_t sum_quantity{0};
        double sum_price{0.0};
        double min_price{std::numeric_limits<double>::infinity()};
        double max_price{-std::numeric_limits<double>::infinity()};

        [[nodiscard]] double avg_price() const noexcept {
            return count > 0 ? (sum_price / static_cast<double>(count)) : 0.0;
        }

        void update(std::int32_t qty, float price) noexcept {
            ++count;
            sum_quantity += qty;
            const double p = static_cast<double>(price);
            sum_price += p;
            if (p < min_price) min_price = p;
            if (p > max_price) max_price = p;
        }
    };

    struct GroupResultRow {
        std::int32_t category_id;
        GroupMetrics metrics;
    };

    class SeqGroupBy {
    public:
        explicit SeqGroupBy(const storage::ColumnarTable& table) noexcept;

        // Group by CategoryID over full table
        [[nodiscard]] std::unordered_map<std::int32_t, GroupMetrics> group_by_category() const;

        // Group by CategoryID over selection vector
        [[nodiscard]] std::unordered_map<std::int32_t, GroupMetrics> group_by_category(
            const std::vector<row_id_t>& selection) const;

        // Returns sorted list of grouped rows (ordered by CategoryID ascending)
        [[nodiscard]] std::vector<GroupResultRow> group_by_category_sorted() const;
        [[nodiscard]] std::vector<GroupResultRow> group_by_category_sorted(
            const std::vector<row_id_t>& selection) const;

    private:
        const storage::ColumnarTable& table_;
    };

} // namespace pqe::execution::sequential
