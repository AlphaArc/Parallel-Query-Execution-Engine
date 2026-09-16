#include "pqe/layer3_execution/sequential/seq_aggregator.hpp"

#include <algorithm>

namespace pqe::execution::sequential {

    SeqAggregator::SeqAggregator(const storage::ColumnarTable& table) noexcept
        : table_(table) {}

    std::size_t SeqAggregator::count() const noexcept {
        return table_.row_count();
    }

    std::size_t SeqAggregator::count(const std::vector<row_id_t>& selection) const noexcept {
        return selection.size();
    }

    double SeqAggregator::sum_price() const noexcept {
        double total = 0.0;
        const auto prices = table_.prices();
        for (float p : prices) {
            total += static_cast<double>(p);
        }
        return total;
    }

    double SeqAggregator::sum_price(const std::vector<row_id_t>& selection) const noexcept {
        double total = 0.0;
        const auto prices = table_.prices();
        for (row_id_t r : selection) {
            total += static_cast<double>(prices[r]);
        }
        return total;
    }

    std::int64_t SeqAggregator::sum_quantity() const noexcept {
        std::int64_t total = 0;
        const auto quantities = table_.quantities();
        for (std::int32_t q : quantities) {
            total += q;
        }
        return total;
    }

    std::int64_t SeqAggregator::sum_quantity(const std::vector<row_id_t>& selection) const noexcept {
        std::int64_t total = 0;
        const auto quantities = table_.quantities();
        for (row_id_t r : selection) {
            total += quantities[r];
        }
        return total;
    }

    ScalarAggregateResult SeqAggregator::aggregate_price() const noexcept {
        ScalarAggregateResult res{};
        const auto prices = table_.prices();
        const std::size_t n = prices.size();

        if (n == 0) {
            res.min = 0.0;
            res.max = 0.0;
            return res;
        }

        res.count = n;
        double sum = 0.0;
        double min_val = static_cast<double>(prices[0]);
        double max_val = static_cast<double>(prices[0]);

        for (std::size_t i = 0; i < n; ++i) {
            const double val = static_cast<double>(prices[i]);
            sum += val;
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }

        res.sum = sum;
        res.avg = sum / static_cast<double>(n);
        res.min = min_val;
        res.max = max_val;
        return res;
    }

    ScalarAggregateResult SeqAggregator::aggregate_price(const std::vector<row_id_t>& selection) const noexcept {
        ScalarAggregateResult res{};
        if (selection.empty()) {
            res.min = 0.0;
            res.max = 0.0;
            return res;
        }

        const auto prices = table_.prices();
        res.count = selection.size();
        double sum = 0.0;
        double min_val = static_cast<double>(prices[selection[0]]);
        double max_val = static_cast<double>(prices[selection[0]]);

        for (row_id_t r : selection) {
            const double val = static_cast<double>(prices[r]);
            sum += val;
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }

        res.sum = sum;
        res.avg = sum / static_cast<double>(res.count);
        res.min = min_val;
        res.max = max_val;
        return res;
    }

    double SeqAggregator::sum_net_sales() const noexcept {
        double total = 0.0;
        const auto prices = table_.prices();
        const auto discounts = table_.discounts();
        const auto quantities = table_.quantities();
        const std::size_t n = prices.size();

        for (std::size_t i = 0; i < n; ++i) {
            const double net_price = static_cast<double>(prices[i]) * (1.0 - static_cast<double>(discounts[i]));
            total += net_price * static_cast<double>(quantities[i]);
        }
        return total;
    }

    double SeqAggregator::sum_net_sales(const std::vector<row_id_t>& selection) const noexcept {
        double total = 0.0;
        const auto prices = table_.prices();
        const auto discounts = table_.discounts();
        const auto quantities = table_.quantities();

        for (row_id_t r : selection) {
            const double net_price = static_cast<double>(prices[r]) * (1.0 - static_cast<double>(discounts[r]));
            total += net_price * static_cast<double>(quantities[r]);
        }
        return total;
    }

} // namespace pqe::execution::sequential
