#include "pqe/layer3_execution/parallel/omp_group_by.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"

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
        
        std::vector<std::unordered_map<std::int32_t, GroupMetrics>> thread_maps;
        
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            int num_threads = omp_get_num_threads();
            
            #pragma omp single
            {
                thread_maps.resize(num_threads);
            }
            
            std::unordered_map<std::int32_t, GroupMetrics> local_map;
            // Preallocate to avoid rehashes if possible.
            local_map.reserve(1024); 
            
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    local_map[cats[r]].update(qty[r], prices[r]);
                }
            }
            
            thread_maps[thread_id] = std::move(local_map);
        }
        
        // Merge thread local maps into a global map sequentially
        std::unordered_map<std::int32_t, GroupMetrics> global_map;
        for (const auto& t_map : thread_maps) {
            for (const auto& [cat_id, metrics] : t_map) {
                auto& gm = global_map[cat_id];
                gm.count += metrics.count;
                gm.sum_quantity += metrics.sum_quantity;
                gm.sum_price += metrics.sum_price;
                if (metrics.min_price < gm.min_price) gm.min_price = metrics.min_price;
                if (metrics.max_price > gm.max_price) gm.max_price = metrics.max_price;
            }
        }
        
        return global_map;
    }

    std::unordered_map<std::int32_t, GroupMetrics> OmpGroupBy::group_by_category(
        const std::vector<row_id_t>& selection) const {
        const auto cats = table_.category_ids();
        const auto qty = table_.quantities();
        const auto prices = table_.prices();
        
        std::vector<std::unordered_map<std::int32_t, GroupMetrics>> thread_maps;
        
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            int num_threads = omp_get_num_threads();
            
            #pragma omp single
            {
                thread_maps.resize(num_threads);
            }
            
            std::unordered_map<std::int32_t, GroupMetrics> local_map;
            local_map.reserve(1024);
            
            #pragma omp for schedule(dynamic, 10000)
            for (std::size_t i = 0; i < selection.size(); ++i) {
                row_id_t r = selection[i];
                local_map[cats[r]].update(qty[r], prices[r]);
            }
            
            thread_maps[thread_id] = std::move(local_map);
        }
        
        // Merge thread local maps into a global map
        std::unordered_map<std::int32_t, GroupMetrics> global_map;
        for (const auto& t_map : thread_maps) {
            for (const auto& [cat_id, metrics] : t_map) {
                auto& gm = global_map[cat_id];
                gm.count += metrics.count;
                gm.sum_quantity += metrics.sum_quantity;
                gm.sum_price += metrics.sum_price;
                if (metrics.min_price < gm.min_price) gm.min_price = metrics.min_price;
                if (metrics.max_price > gm.max_price) gm.max_price = metrics.max_price;
            }
        }
        
        return global_map;
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
