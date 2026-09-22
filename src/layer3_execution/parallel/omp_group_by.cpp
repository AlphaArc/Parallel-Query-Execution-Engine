#include "pqe/layer3_execution/parallel/omp_group_by.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"

#include "pqe/layer4_concurrency/concurrent_hash_map.hpp"

#include <omp.h>
#include <algorithm>

namespace pqe::execution::parallel {

    OmpGroupBy::OmpGroupBy(const storage::ColumnarTable& table) noexcept
        : table_(table) {}

    std::unordered_map<std::int32_t, GroupMetrics> OmpGroupBy::group_by_category() const {
        const auto cats = table_.category_ids();
        const auto qty = table_.quantities();
        const auto prices = table_.prices();
        
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        concurrency::ConcurrentShardedMap<std::int32_t, GroupMetrics> shared_map;
        
        #pragma omp parallel
        {
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    shared_map.update(cats[r], [q = qty[r], p = prices[r]](GroupMetrics& gm) {
                        gm.update(q, p);
                    });
                }
            }
        }
        
        std::unordered_map<std::int32_t, GroupMetrics> final_map;
        for (const auto& [k, v] : shared_map.gather_all()) {
            final_map[k] = v;
        }
        
        return final_map;
    }

    std::unordered_map<std::int32_t, GroupMetrics> OmpGroupBy::group_by_category(
        const std::vector<row_id_t>& selection) const {
        const auto cats = table_.category_ids();
        const auto qty = table_.quantities();
        const auto prices = table_.prices();
        
        concurrency::ConcurrentShardedMap<std::int32_t, GroupMetrics> shared_map;
        
        #pragma omp parallel
        {
            #pragma omp for schedule(dynamic, 10000)
            for (std::size_t i = 0; i < selection.size(); ++i) {
                row_id_t r = selection[i];
                shared_map.update(cats[r], [q = qty[r], p = prices[r]](GroupMetrics& gm) {
                    gm.update(q, p);
                });
            }
        }
        
        std::unordered_map<std::int32_t, GroupMetrics> final_map;
        for (const auto& [k, v] : shared_map.gather_all()) {
            final_map[k] = v;
        }
        
        return final_map;
    }

    std::vector<GroupResultRow> OmpGroupBy::group_by_category_sorted() const {
        auto map = group_by_category();
        std::vector<GroupResultRow> result;
        result.reserve(map.size());
        for (const auto& [cat_id, metrics] : map) {
            result.push_back({cat_id, metrics});
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.category_id < b.category_id;
        });
        return result;
    }

    std::vector<GroupResultRow> OmpGroupBy::group_by_category_sorted(
        const std::vector<row_id_t>& selection) const {
        auto map = group_by_category(selection);
        std::vector<GroupResultRow> result;
        result.reserve(map.size());
        for (const auto& [cat_id, metrics] : map) {
            result.push_back({cat_id, metrics});
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.category_id < b.category_id;
        });
        return result;
    }

} // namespace pqe::execution::parallel
