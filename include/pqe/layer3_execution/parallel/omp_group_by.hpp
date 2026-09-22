#pragma once

#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"
#include "pqe/layer3_execution/sequential/seq_group_by.hpp" // For GroupMetrics, GroupResultRow

#include <unordered_map>
#include <vector>
#include <cstdint>

namespace pqe::execution::parallel {

    using sequential::GroupMetrics;
    using sequential::GroupResultRow;

    class OmpGroupBy {
    public:
        explicit OmpGroupBy(const storage::ColumnarTable& table) noexcept;

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

} // namespace pqe::execution::parallel
