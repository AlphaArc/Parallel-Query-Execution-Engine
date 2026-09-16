#include "pqe/layer3_execution/sequential/seq_filter.hpp"

namespace pqe::execution::sequential {

    SeqFilter::SeqFilter(const storage::ColumnarTable& table) noexcept
        : table_(table) {}

    std::vector<row_id_t> SeqFilter::filter(
        const std::function<bool(row_id_t)>& predicate) const {
        std::vector<row_id_t> result;
        const std::size_t total = table_.row_count();
        result.reserve(total / 4); // Initial capacity heuristic

        for (row_id_t r = 0; r < total; ++r) {
            if (predicate(r)) {
                result.push_back(r);
            }
        }
        return result;
    }

    std::vector<row_id_t> SeqFilter::filter_subset(
        const std::vector<row_id_t>& input_selection,
        const std::function<bool(row_id_t)>& predicate) const {
        std::vector<row_id_t> result;
        result.reserve(input_selection.size() / 2);

        for (row_id_t r : input_selection) {
            if (predicate(r)) {
                result.push_back(r);
            }
        }
        return result;
    }

    std::vector<row_id_t> SeqFilter::filter_quantity_gt(std::int32_t threshold) const {
        std::vector<row_id_t> result;
        const auto qty = table_.quantities();
        const std::size_t total = qty.size();
        result.reserve(total / 2);

        for (row_id_t r = 0; r < total; ++r) {
            if (qty[r] > threshold) {
                result.push_back(r);
            }
        }
        return result;
    }

    std::vector<row_id_t> SeqFilter::filter_category_eq(std::int32_t target_category) const {
        std::vector<row_id_t> result;
        const auto cats = table_.category_ids();
        const std::size_t total = cats.size();
        result.reserve(total / 50 + 100);

        for (row_id_t r = 0; r < total; ++r) {
            if (cats[r] == target_category) {
                result.push_back(r);
            }
        }
        return result;
    }

    std::vector<row_id_t> SeqFilter::filter_quantity_and_category(
        std::int32_t quantity_gt, std::int32_t category_eq) const {
        std::vector<row_id_t> result;
        const auto qty = table_.quantities();
        const auto cats = table_.category_ids();
        const std::size_t total = qty.size();
        result.reserve(total / 100 + 100);

        for (row_id_t r = 0; r < total; ++r) {
            if (qty[r] > quantity_gt && cats[r] == category_eq) {
                result.push_back(r);
            }
        }
        return result;
    }

} // namespace pqe::execution::sequential
