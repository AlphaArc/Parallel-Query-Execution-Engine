#include "pqe/layer3_execution/parallel/omp_aggregator.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"

#include "pqe/layer4_concurrency/atomic_accumulator.hpp"

#include <omp.h>
#include <immintrin.h>
#include <algorithm>
#include <limits>

namespace {
    inline float hsum_ps(__m256 v) {
        __m128 vlow = _mm256_castps256_ps128(v);
        __m128 vhigh = _mm256_extractf128_ps(v, 1);
        vlow = _mm_add_ps(vlow, vhigh);
        __m128 shuf = _mm_movehdup_ps(vlow);
        __m128 sums = _mm_add_ps(vlow, shuf);
        shuf = _mm_movehl_ps(shuf, sums);
        sums = _mm_add_ss(sums, shuf);
        return _mm_cvtss_f32(sums);
    }
    
    inline float hmin_ps(__m256 v) {
        __m128 vlow = _mm256_castps256_ps128(v);
        __m128 vhigh = _mm256_extractf128_ps(v, 1);
        vlow = _mm_min_ps(vlow, vhigh);
        __m128 shuf = _mm_movehdup_ps(vlow);
        __m128 mins = _mm_min_ps(vlow, shuf);
        shuf = _mm_movehl_ps(shuf, mins);
        mins = _mm_min_ss(mins, shuf);
        return _mm_cvtss_f32(mins);
    }

    inline float hmax_ps(__m256 v) {
        __m128 vlow = _mm256_castps256_ps128(v);
        __m128 vhigh = _mm256_extractf128_ps(v, 1);
        vlow = _mm_max_ps(vlow, vhigh);
        __m128 shuf = _mm_movehdup_ps(vlow);
        __m128 maxs = _mm_max_ps(vlow, shuf);
        shuf = _mm_movehl_ps(shuf, maxs);
        maxs = _mm_max_ss(maxs, shuf);
        return _mm_cvtss_f32(maxs);
    }
}
#include <algorithm>
#include <limits>

namespace pqe::execution::parallel {

    OmpAggregator::OmpAggregator(const storage::ColumnarTable& table) noexcept
        : table_(table) {}

    std::size_t OmpAggregator::count() const noexcept {
        return table_.row_count();
    }

    std::size_t OmpAggregator::count(const std::vector<row_id_t>& selection) const noexcept {
        return selection.size();
    }

    double OmpAggregator::sum_price() const noexcept {
        const auto prices = table_.prices();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        concurrency::AtomicAccumulator<double> global_sum(0.0);

        #pragma omp parallel
        {
            double local_sum = 0.0;
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    local_sum += prices[r];
                }
            }
            global_sum.add(local_sum);
        }
        return global_sum.load();
    }

    double OmpAggregator::sum_price(const std::vector<row_id_t>& selection) const noexcept {
        const auto prices = table_.prices();
        concurrency::AtomicAccumulator<double> global_sum(0.0);

        #pragma omp parallel
        {
            double local_sum = 0.0;
            #pragma omp for schedule(dynamic, 10000)
            for (std::size_t i = 0; i < selection.size(); ++i) {
                local_sum += prices[selection[i]];
            }
            global_sum.add(local_sum);
        }
        return global_sum.load();
    }

    std::int64_t OmpAggregator::sum_quantity() const noexcept {
        const auto qty = table_.quantities();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        concurrency::AtomicAccumulator<std::int64_t> global_sum(0);

        #pragma omp parallel
        {
            std::int64_t local_sum = 0;
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    local_sum += qty[r];
                }
            }
            global_sum.add(local_sum);
        }
        return global_sum.load();
    }

    std::int64_t OmpAggregator::sum_quantity(const std::vector<row_id_t>& selection) const noexcept {
        const auto qty = table_.quantities();
        concurrency::AtomicAccumulator<std::int64_t> global_sum(0);

        #pragma omp parallel
        {
            std::int64_t local_sum = 0;
            #pragma omp for schedule(dynamic, 10000)
            for (std::size_t i = 0; i < selection.size(); ++i) {
                local_sum += qty[selection[i]];
            }
            global_sum.add(local_sum);
        }
        return global_sum.load();
    }

    ScalarAggregateResult OmpAggregator::aggregate_price() const noexcept {
        const auto prices = table_.prices();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);

        concurrency::AtomicAccumulator<std::size_t> total_count(0);
        concurrency::AtomicAccumulator<double> total_sum(0.0);
        concurrency::AtomicAccumulator<double> global_min(std::numeric_limits<double>::infinity());
        concurrency::AtomicAccumulator<double> global_max(-std::numeric_limits<double>::infinity());

        #pragma omp parallel
        {
            std::size_t local_count = 0;
            double local_sum = 0.0;
            double local_min = std::numeric_limits<double>::infinity();
            double local_max = -std::numeric_limits<double>::infinity();

            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    double p = prices[r];
                    local_count++;
                    local_sum += p;
                    if (p < local_min) local_min = p;
                    if (p > local_max) local_max = p;
                }
            }

            total_count.add(local_count);
            total_sum.add(local_sum);
            global_min.update_min(local_min);
            global_max.update_max(local_max);
        }

        ScalarAggregateResult res;
        res.count = total_count.load();
        res.sum = total_sum.load();
        res.avg = (res.count > 0) ? (res.sum / static_cast<double>(res.count)) : 0.0;
        res.min = global_min.load();
        res.max = global_max.load();
        return res;
    }

    ScalarAggregateResult OmpAggregator::aggregate_price(const std::vector<row_id_t>& selection) const noexcept {
        const auto prices = table_.prices();

        concurrency::AtomicAccumulator<std::size_t> total_count(0);
        concurrency::AtomicAccumulator<double> total_sum(0.0);
        concurrency::AtomicAccumulator<double> global_min(std::numeric_limits<double>::infinity());
        concurrency::AtomicAccumulator<double> global_max(-std::numeric_limits<double>::infinity());

        #pragma omp parallel
        {
            std::size_t local_count = 0;
            double local_sum = 0.0;
            double local_min = std::numeric_limits<double>::infinity();
            double local_max = -std::numeric_limits<double>::infinity();

            #pragma omp for schedule(dynamic, 10000)
            for (std::size_t i = 0; i < selection.size(); ++i) {
                double p = prices[selection[i]];
                local_count++;
                local_sum += p;
                if (p < local_min) local_min = p;
                if (p > local_max) local_max = p;
            }

            total_count.add(local_count);
            total_sum.add(local_sum);
            global_min.update_min(local_min);
            global_max.update_max(local_max);
        }

        ScalarAggregateResult res;
        res.count = total_count.load();
        res.sum = total_sum.load();
        res.avg = (res.count > 0) ? (res.sum / static_cast<double>(res.count)) : 0.0;
        res.min = global_min.load();
        res.max = global_max.load();
        return res;
    }

    double OmpAggregator::sum_net_sales() const noexcept {
        const auto prices = table_.prices();
        const auto discounts = table_.discounts();
        const auto qty = table_.quantities();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        concurrency::AtomicAccumulator<double> global_sum(0.0);

        #pragma omp parallel
        {
            double local_sum = 0.0;
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    local_sum += static_cast<double>(prices[r]) * (1.0 - static_cast<double>(discounts[r])) * static_cast<double>(qty[r]);
                }
            }
            global_sum.add(local_sum);
        }
        return global_sum.load();
    }

    double OmpAggregator::sum_net_sales(const std::vector<row_id_t>& selection) const noexcept {
        const auto prices = table_.prices();
        const auto discounts = table_.discounts();
        const auto qty = table_.quantities();
        concurrency::AtomicAccumulator<double> global_sum(0.0);

        #pragma omp parallel
        {
            double local_sum = 0.0;
            #pragma omp for schedule(dynamic, 10000)
            for (std::size_t i = 0; i < selection.size(); ++i) {
                row_id_t r = selection[i];
                local_sum += static_cast<double>(prices[r]) * (1.0 - static_cast<double>(discounts[r])) * static_cast<double>(qty[r]);
            }
            global_sum.add(local_sum);
        }
        return global_sum.load();
    }

    // =========================================================================
    // AVX2 HARDWARE SIMD VECTORIZATION IMPLEMENTATIONS
    // =========================================================================

    ScalarAggregateResult OmpAggregator::aggregate_price_simd() const noexcept {
        const auto prices = table_.prices();
        const std::size_t total_rows = table_.row_count();
        
        concurrency::AtomicAccumulator<std::size_t> total_count(0);
        concurrency::AtomicAccumulator<double> total_sum(0.0);
        concurrency::AtomicAccumulator<double> global_min(std::numeric_limits<double>::infinity());
        concurrency::AtomicAccumulator<double> global_max(-std::numeric_limits<double>::infinity());

        #pragma omp parallel
        {
            std::size_t local_count = 0;
            double local_sum = 0.0;
            double local_min = std::numeric_limits<double>::infinity();
            double local_max = -std::numeric_limits<double>::infinity();

            __m256 v_sum = _mm256_setzero_ps();
            __m256 v_min = _mm256_set1_ps(std::numeric_limits<float>::infinity());
            __m256 v_max = _mm256_set1_ps(-std::numeric_limits<float>::infinity());

            #pragma omp for schedule(static)
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(total_rows); i += 8) {
                if (i + 8 <= static_cast<std::int64_t>(total_rows)) {
                    __m256 v_prices = _mm256_loadu_ps(&prices[i]);
                    v_sum = _mm256_add_ps(v_sum, v_prices);
                    v_min = _mm256_min_ps(v_min, v_prices);
                    v_max = _mm256_max_ps(v_max, v_prices);
                    local_count += 8;
                } else {
                    for (std::size_t j = i; j < total_rows; ++j) {
                        float p = prices[j];
                        local_sum += p;
                        if (p < local_min) local_min = p;
                        if (p > local_max) local_max = p;
                        local_count++;
                    }
                }
            }
            
            local_sum += static_cast<double>(hsum_ps(v_sum));
            
            float f_min = hmin_ps(v_min);
            if (f_min < local_min) local_min = static_cast<double>(f_min);
            
            float f_max = hmax_ps(v_max);
            if (f_max > local_max) local_max = static_cast<double>(f_max);

            total_count.add(local_count);
            total_sum.add(local_sum);
            global_min.update_min(local_min);
            global_max.update_max(local_max);
        }

        ScalarAggregateResult res;
        res.count = total_count.load();
        res.sum = total_sum.load();
        res.avg = (res.count > 0) ? (res.sum / static_cast<double>(res.count)) : 0.0;
        res.min = global_min.load();
        res.max = global_max.load();
        return res;
    }

    ScalarAggregateResult OmpAggregator::aggregate_price_simd(const std::vector<row_id_t>& selection) const noexcept {
        const auto prices = table_.prices();
        const std::size_t sel_size = selection.size();

        concurrency::AtomicAccumulator<std::size_t> total_count(0);
        concurrency::AtomicAccumulator<double> total_sum(0.0);
        concurrency::AtomicAccumulator<double> global_min(std::numeric_limits<double>::infinity());
        concurrency::AtomicAccumulator<double> global_max(-std::numeric_limits<double>::infinity());

        #pragma omp parallel
        {
            std::size_t local_count = 0;
            double local_sum = 0.0;
            double local_min = std::numeric_limits<double>::infinity();
            double local_max = -std::numeric_limits<double>::infinity();

            __m256 v_sum = _mm256_setzero_ps();
            __m256 v_min = _mm256_set1_ps(std::numeric_limits<float>::infinity());
            __m256 v_max = _mm256_set1_ps(-std::numeric_limits<float>::infinity());

            #pragma omp for schedule(static)
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(sel_size); i += 8) {
                if (i + 8 <= static_cast<std::int64_t>(sel_size)) {
                    // Gather scattered elements into SIMD vector using VPGATHERDD logic
                    __m256i v_idx = _mm256_loadu_si256((const __m256i*)&selection[i]);
                    __m256 v_prices = _mm256_i32gather_ps(&prices[0], v_idx, 4);

                    v_sum = _mm256_add_ps(v_sum, v_prices);
                    v_min = _mm256_min_ps(v_min, v_prices);
                    v_max = _mm256_max_ps(v_max, v_prices);
                    local_count += 8;
                } else {
                    for (std::size_t j = i; j < sel_size; ++j) {
                        float p = prices[selection[j]];
                        local_sum += p;
                        if (p < local_min) local_min = p;
                        if (p > local_max) local_max = p;
                        local_count++;
                    }
                }
            }

            local_sum += static_cast<double>(hsum_ps(v_sum));
            float f_min = hmin_ps(v_min);
            if (f_min < local_min) local_min = static_cast<double>(f_min);
            float f_max = hmax_ps(v_max);
            if (f_max > local_max) local_max = static_cast<double>(f_max);

            total_count.add(local_count);
            total_sum.add(local_sum);
            global_min.update_min(local_min);
            global_max.update_max(local_max);
        }

        ScalarAggregateResult res;
        res.count = total_count.load();
        res.sum = total_sum.load();
        res.avg = (res.count > 0) ? (res.sum / static_cast<double>(res.count)) : 0.0;
        res.min = global_min.load();
        res.max = global_max.load();
        return res;
    }

    double OmpAggregator::sum_net_sales_simd() const noexcept {
        const auto prices = table_.prices();
        const auto discounts = table_.discounts();
        const auto qty = table_.quantities();
        const std::size_t total_rows = table_.row_count();
        
        concurrency::AtomicAccumulator<double> global_sum(0.0);

        #pragma omp parallel
        {
            double local_sum = 0.0;
            __m256 v_sum = _mm256_setzero_ps();
            __m256 v_ones = _mm256_set1_ps(1.0f);

            #pragma omp for schedule(static)
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(total_rows); i += 8) {
                if (i + 8 <= static_cast<std::int64_t>(total_rows)) {
                    __m256 v_p = _mm256_loadu_ps(&prices[i]);
                    __m256 v_d = _mm256_loadu_ps(&discounts[i]);
                    __m256i v_q_int = _mm256_loadu_si256((const __m256i*)&qty[i]);
                    __m256 v_q = _mm256_cvtepi32_ps(v_q_int);
                    
                    __m256 v_disc = _mm256_sub_ps(v_ones, v_d);
                    __m256 v_prod1 = _mm256_mul_ps(v_p, v_disc);
                    __m256 v_sales = _mm256_mul_ps(v_prod1, v_q);
                    
                    v_sum = _mm256_add_ps(v_sum, v_sales);
                } else {
                    for (std::size_t j = i; j < total_rows; ++j) {
                        local_sum += static_cast<double>(prices[j]) * (1.0 - static_cast<double>(discounts[j])) * static_cast<double>(qty[j]);
                    }
                }
            }
            
            local_sum += static_cast<double>(hsum_ps(v_sum));
            global_sum.add(local_sum);
        }
        return global_sum.load();
    }

    double OmpAggregator::sum_net_sales_simd(const std::vector<row_id_t>& selection) const noexcept {
        const auto prices = table_.prices();
        const auto discounts = table_.discounts();
        const auto qty = table_.quantities();
        const std::size_t sel_size = selection.size();
        
        concurrency::AtomicAccumulator<double> global_sum(0.0);

        #pragma omp parallel
        {
            double local_sum = 0.0;
            __m256 v_sum = _mm256_setzero_ps();
            __m256 v_ones = _mm256_set1_ps(1.0f);

            #pragma omp for schedule(static)
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(sel_size); i += 8) {
                if (i + 8 <= static_cast<std::int64_t>(sel_size)) {
                    __m256i v_idx = _mm256_loadu_si256((const __m256i*)&selection[i]);
                    
                    __m256 v_p = _mm256_i32gather_ps(&prices[0], v_idx, 4);
                    __m256 v_d = _mm256_i32gather_ps(&discounts[0], v_idx, 4);
                    __m256i v_q_int = _mm256_i32gather_epi32(&qty[0], v_idx, 4);
                    __m256 v_q = _mm256_cvtepi32_ps(v_q_int);
                    
                    __m256 v_disc = _mm256_sub_ps(v_ones, v_d);
                    __m256 v_prod1 = _mm256_mul_ps(v_p, v_disc);
                    __m256 v_sales = _mm256_mul_ps(v_prod1, v_q);
                    
                    v_sum = _mm256_add_ps(v_sum, v_sales);
                } else {
                    for (std::size_t j = i; j < sel_size; ++j) {
                        row_id_t r = selection[j];
                        local_sum += static_cast<double>(prices[r]) * (1.0 - static_cast<double>(discounts[r])) * static_cast<double>(qty[r]);
                    }
                }
            }
            local_sum += static_cast<double>(hsum_ps(v_sum));
            global_sum.add(local_sum);
        }
        return global_sum.load();
    }

} // namespace pqe::execution::parallel
