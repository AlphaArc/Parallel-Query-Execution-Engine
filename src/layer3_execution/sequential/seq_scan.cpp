#include "pqe/layer3_execution/sequential/seq_scan.hpp"

namespace pqe::execution::sequential {

    SeqScan::SeqScan(const storage::ColumnarTable& table) noexcept
        : table_(table), start_row_(0), end_row_(table.row_count()), current_idx_(0) {}

    SeqScan::SeqScan(const storage::ColumnarTable& table, row_id_t start_row, row_id_t end_row) noexcept
        : table_(table),
          start_row_(start_row),
          end_row_(std::min(end_row, table.row_count())),
          current_idx_(start_row) {}

    SeqScan::SeqScan(const storage::ColumnarTable& table, const MorselDesc& morsel) noexcept
        : table_(table),
          start_row_(morsel.start_row),
          end_row_(std::min(morsel.end_row, table.row_count())),
          current_idx_(morsel.start_row) {}

    void SeqScan::reset() noexcept {
        current_idx_ = start_row_;
    }

    bool SeqScan::has_next() const noexcept {
        return current_idx_ < end_row_;
    }

    row_id_t SeqScan::next() noexcept {
        return current_idx_++;
    }

    void SeqScan::for_each(const std::function<void(row_id_t)>& callback) {
        for (row_id_t r = start_row_; r < end_row_; ++r) {
            callback(r);
        }
    }

} // namespace pqe::execution::sequential
