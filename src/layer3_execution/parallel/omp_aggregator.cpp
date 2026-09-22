#include "pqe/layer3_execution/parallel/omp_aggregator.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"

#include <omp.h>
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
        double global_sum = 0.0;

        #pragma omp parallel
        {
            double local_sum = 0.0;
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    local_sum += prices[r];
                }
            }
            #pragma omp atomic
            global_sum += local_sum;
        }
        return global_sum;
    }

    double OmpAggregator::sum_price(const std::vector<row_id_t>& selection) const noexcept {
        const auto prices = table_.prices();
        double global_sum = 0.0;

        #pragma omp parallel for reduction(+:global_sum) schedule(dynamic, 10000)
        for (std::size_t i = 0; i < selection.size(); ++i) {
            global_sum += prices[selection[i]];
        }
        return global_sum;
    }

    std::int64_t OmpAggregator::sum_quantity() const noexcept {
        const auto qty = table_.quantities();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        std::int64_t global_sum = 0;

        #pragma omp parallel
        {
            std::int64_t local_sum = 0;
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    local_sum += qty[r];
                }
            }
            #pragma omp atomic
            global_sum += local_sum;
        }
        return global_sum;
    }

    std::int64_t OmpAggregator::sum_quantity(const std::vector<row_id_t>& selection) const noexcept {
        const auto qty = table_.quantities();
        std::int64_t global_sum = 0;

        #pragma omp parallel for reduction(+:global_sum) schedule(dynamic, 10000)
        for (std::size_t i = 0; i < selection.size(); ++i) {
            global_sum += qty[selection[i]];
        }
        return global_sum;
    }

    ScalarAggregateResult OmpAggregator::aggregate_price() const noexcept {
        const auto prices = table_.prices();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);

        std::size_t total_count = 0;
        double total_sum = 0.0;
        double global_min = std::numeric_limits<double>::infinity();
        double global_max = -std::numeric_limits<double>::infinity();

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

            #pragma omp critical
            {
                total_count += local_count;
                total_sum += local_sum;
                if (local_min < global_min) global_min = local_min;
                if (local_max > global_max) global_max = local_max;
            }
        }

        ScalarAggregateResult res;
        res.count = total_count;
        res.sum = total_sum;
        res.avg = (total_count > 0) ? (total_sum / static_cast<double>(total_count)) : 0.0;
        res.min = global_min;
        res.max = global_max;
        return res;
    }

    ScalarAggregateResult OmpAggregator::aggregate_price(const std::vector<row_id_t>& selection) const noexcept {
        const auto prices = table_.prices();

        std::size_t total_count = 0;
        double total_sum = 0.0;
        double global_min = std::numeric_limits<double>::infinity();
        double global_max = -std::numeric_limits<double>::infinity();

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

            #pragma omp critical
            {
                total_count += local_count;
                total_sum += local_sum;
                if (local_min < global_min) global_min = local_min;
                if (local_max > global_max) global_max = local_max;
            }
        }

        ScalarAggregateResult res;
        res.count = total_count;
        res.sum = total_sum;
        res.avg = (total_count > 0) ? (total_sum / static_cast<double>(total_count)) : 0.0;
        res.min = global_min;
        res.max = global_max;
        return res;
    }

    double OmpAggregator::sum_net_sales() const noexcept {
        const auto prices = table_.prices();
        const auto discounts = table_.discounts();
        const auto qty = table_.quantities();
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);
        double global_sum = 0.0;

        #pragma omp parallel
        {
            double local_sum = 0.0;
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    local_sum += static_cast<double>(prices[r]) * (1.0 - static_cast<double>(discounts[r])) * static_cast<double>(qty[r]);
                }
            }
            #pragma omp atomic
            global_sum += local_sum;
        }
        return global_sum;
    }

    double OmpAggregator::sum_net_sales(const std::vector<row_id_t>& selection) const noexcept {
        const auto prices = table_.prices();
        const auto discounts = table_.discounts();
        const auto qty = table_.quantities();
        double global_sum = 0.0;

        #pragma omp parallel for reduction(+:global_sum) schedule(dynamic, 10000)
        for (std::size_t i = 0; i < selection.size(); ++i) {
            row_id_t r = selection[i];
            global_sum += static_cast<double>(prices[r]) * (1.0 - static_cast<double>(discounts[r])) * static_cast<double>(qty[r]);
        }
        return global_sum;
    }

} // namespace pqe::execution::parallel
