#pragma once

#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"

#include <cstddef>
#include <functional>

namespace pqe::execution::sequential {

    class SeqScan {
    public:
        explicit SeqScan(const storage::ColumnarTable& table) noexcept;
        SeqScan(const storage::ColumnarTable& table, row_id_t start_row, row_id_t end_row) noexcept;
        explicit SeqScan(const storage::ColumnarTable& table, const MorselDesc& morsel) noexcept;

        // Reset scan position
        void reset() noexcept;

        // Moves to next row; returns true if valid
        bool has_next() const noexcept;
        row_id_t next() noexcept;

        // Current row index
        [[nodiscard]] row_id_t current_row() const noexcept { return current_idx_; }
        [[nodiscard]] std::size_t total_rows_to_scan() const noexcept { return end_row_ - start_row_; }

        // Functional scan: executes a callback per row
        void for_each(const std::function<void(row_id_t)>& callback);

    private:
        const storage::ColumnarTable& table_;
        row_id_t start_row_{0};
        row_id_t end_row_{0};
        row_id_t current_idx_{0};
    };

} // namespace pqe::execution::sequential
