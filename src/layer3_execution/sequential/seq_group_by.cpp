#include "pqe/layer3_execution/sequential/seq_group_by.hpp"

#include <algorithm>

namespace pqe::execution::sequential {

    SeqGroupBy::SeqGroupBy(const storage::ColumnarTable& table) noexcept
        : table_(table) {}

    std::unordered_map<std::int32_t, GroupMetrics> SeqGroupBy::group_by_category() const {
        std::unordered_map<std::int32_t, GroupMetrics> groups;
        groups.reserve(64); // Sales dataset schema uses categories 1..50

        const auto cats = table_.category_ids();
        const auto qty = table_.quantities();
        const auto prices = table_.prices();
        const std::size_t total = cats.size();

        for (std::size_t r = 0; r < total; ++r) {
            groups[cats[r]].update(qty[r], prices[r]);
        }

        return groups;
    }

    std::unordered_map<std::int32_t, GroupMetrics> SeqGroupBy::group_by_category(
        const std::vector<row_id_t>& selection) const {
        std::unordered_map<std::int32_t, GroupMetrics> groups;
        groups.reserve(64);

        const auto cats = table_.category_ids();
        const auto qty = table_.quantities();
        const auto prices = table_.prices();

        for (row_id_t r : selection) {
            groups[cats[r]].update(qty[r], prices[r]);
        }

        return groups;
    }

    std::vector<GroupResultRow> SeqGroupBy::group_by_category_sorted() const {
        auto map = group_by_category();
        std::vector<GroupResultRow> rows;
        rows.reserve(map.size());

        for (auto& [cat_id, metrics] : map) {
            rows.push_back(GroupResultRow{cat_id, metrics});
        }

        std::sort(rows.begin(), rows.end(), [](const GroupResultRow& a, const GroupResultRow& b) {
            return a.category_id < b.category_id;
        });

        return rows;
    }

    std::vector<GroupResultRow> SeqGroupBy::group_by_category_sorted(
        const std::vector<row_id_t>& selection) const {
        auto map = group_by_category(selection);
        std::vector<GroupResultRow> rows;
        rows.reserve(map.size());

        for (auto& [cat_id, metrics] : map) {
            rows.push_back(GroupResultRow{cat_id, metrics});
        }

        std::sort(rows.begin(), rows.end(), [](const GroupResultRow& a, const GroupResultRow& b) {
            return a.category_id < b.category_id;
        });

        return rows;
    }

} // namespace pqe::execution::sequential
