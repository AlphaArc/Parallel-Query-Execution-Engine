#include "pqe/layer3_execution/parallel/omp_aggregator.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"

#include "pqe/layer4_concurrency/atomic_accumulator.hpp"

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

} // namespace pqe::execution::parallel
