#include "pqe/layer3_execution/parallel/omp_filter.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"

#include <omp.h>

namespace pqe::execution::parallel {

    OmpFilter::OmpFilter(const storage::ColumnarTable& table) noexcept
        : table_(table) {}

    std::vector<row_id_t> OmpFilter::filter(
        const std::function<bool(row_id_t)>& predicate) const {
        
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        std::vector<std::vector<row_id_t>> thread_results;
        
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            int num_threads = omp_get_num_threads();
            
            #pragma omp single
            {
                thread_results.resize(num_threads);
            }
            
            std::vector<row_id_t> local_res;
            local_res.reserve(allocator.morsel_size());
            
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    if (predicate(r)) {
                        local_res.push_back(r);
                    }
                }
            }
            
            thread_results[thread_id] = std::move(local_res);
        }
        
        std::size_t total = 0;
        for (const auto& tr : thread_results) total += tr.size();
        
        std::vector<row_id_t> final_res;
        final_res.reserve(total);
        for (auto& tr : thread_results) {
            final_res.insert(final_res.end(), std::make_move_iterator(tr.begin()), std::make_move_iterator(tr.end()));
        }
        
        return final_res;
    }

    std::vector<row_id_t> OmpFilter::filter_subset(
        const std::vector<row_id_t>& input_selection,
        const std::function<bool(row_id_t)>& predicate) const {
        
        std::vector<std::vector<row_id_t>> thread_results;
        
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            int num_threads = omp_get_num_threads();
            
            #pragma omp single
            {
                thread_results.resize(num_threads);
            }
            
            std::vector<row_id_t> local_res;
            local_res.reserve(input_selection.size() / num_threads);
            
            #pragma omp for schedule(dynamic, 10000)
            for (std::size_t i = 0; i < input_selection.size(); ++i) {
                row_id_t r = input_selection[i];
                if (predicate(r)) {
                    local_res.push_back(r);
                }
            }
            
            thread_results[thread_id] = std::move(local_res);
        }
        
        std::size_t total = 0;
        for (const auto& tr : thread_results) total += tr.size();
        
        std::vector<row_id_t> final_res;
        final_res.reserve(total);
        for (auto& tr : thread_results) {
            final_res.insert(final_res.end(), std::make_move_iterator(tr.begin()), std::make_move_iterator(tr.end()));
        }
        
        return final_res;
    }

    std::vector<row_id_t> OmpFilter::filter_quantity_gt(std::int32_t threshold) const {
        const auto qty = table_.quantities();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        std::vector<std::vector<row_id_t>> thread_results;
        
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            int num_threads = omp_get_num_threads();
            #pragma omp single
            { thread_results.resize(num_threads); }
            
            std::vector<row_id_t> local_res;
            local_res.reserve(allocator.morsel_size());
            
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    if (qty[r] > threshold) {
                        local_res.push_back(r);
                    }
                }
            }
            thread_results[thread_id] = std::move(local_res);
        }
        
        std::size_t total = 0;
        for (const auto& tr : thread_results) total += tr.size();
        std::vector<row_id_t> final_res;
        final_res.reserve(total);
        for (auto& tr : thread_results) {
            final_res.insert(final_res.end(), std::make_move_iterator(tr.begin()), std::make_move_iterator(tr.end()));
        }
        return final_res;
    }

    std::vector<row_id_t> OmpFilter::filter_category_eq(std::int32_t target_category) const {
        const auto cats = table_.category_ids();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        std::vector<std::vector<row_id_t>> thread_results;
        
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            int num_threads = omp_get_num_threads();
            #pragma omp single
            { thread_results.resize(num_threads); }
            
            std::vector<row_id_t> local_res;
            local_res.reserve(allocator.morsel_size() / 10);
            
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    if (cats[r] == target_category) {
                        local_res.push_back(r);
                    }
                }
            }
            thread_results[thread_id] = std::move(local_res);
        }
        
        std::size_t total = 0;
        for (const auto& tr : thread_results) total += tr.size();
        std::vector<row_id_t> final_res;
        final_res.reserve(total);
        for (auto& tr : thread_results) {
            final_res.insert(final_res.end(), std::make_move_iterator(tr.begin()), std::make_move_iterator(tr.end()));
        }
        return final_res;
    }

    std::vector<row_id_t> OmpFilter::filter_quantity_and_category(
        std::int32_t quantity_gt, std::int32_t category_eq) const {
        const auto qty = table_.quantities();
        const auto cats = table_.category_ids();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        std::vector<std::vector<row_id_t>> thread_results;
        
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            int num_threads = omp_get_num_threads();
            #pragma omp single
            { thread_results.resize(num_threads); }
            
            std::vector<row_id_t> local_res;
            local_res.reserve(allocator.morsel_size() / 10);
            
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    if (qty[r] > quantity_gt && cats[r] == category_eq) {
                        local_res.push_back(r);
                    }
                }
            }
            thread_results[thread_id] = std::move(local_res);
        }
        
        std::size_t total = 0;
        for (const auto& tr : thread_results) total += tr.size();
        std::vector<row_id_t> final_res;
        final_res.reserve(total);
        for (auto& tr : thread_results) {
            final_res.insert(final_res.end(), std::make_move_iterator(tr.begin()), std::make_move_iterator(tr.end()));
        }
        return final_res;
    }

} // namespace pqe::execution::parallel
