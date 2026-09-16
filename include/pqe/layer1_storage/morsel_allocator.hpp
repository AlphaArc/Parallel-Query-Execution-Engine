#pragma once

#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"

#include <vector>
#include <atomic>
#include <optional>
#include <cstddef>

namespace pqe::storage {

    class MorselAllocator {
    public:
        explicit MorselAllocator(std::size_t morsel_size = DEFAULT_MORSEL_SIZE) noexcept;
        MorselAllocator(const ColumnarTable& table, std::size_t morsel_size = DEFAULT_MORSEL_SIZE);
        MorselAllocator(std::size_t total_rows, std::size_t morsel_size);

        // Partition a given total row count into morsels
        void partition(std::size_t total_rows, std::size_t morsel_size = DEFAULT_MORSEL_SIZE);

        // Resets allocation cursor to beginning (for starting a new query phase)
        void reset() noexcept;

        // Thread-safe lock-free extraction of next available morsel
        [[nodiscard]] std::optional<MorselDesc> get_next_morsel() noexcept;

        // Accessors
        [[nodiscard]] std::size_t total_rows() const noexcept { return total_rows_; }
        [[nodiscard]] std::size_t morsel_size() const noexcept { return morsel_size_; }
        [[nodiscard]] std::size_t morsel_count() const noexcept { return morsels_.size(); }
        [[nodiscard]] const std::vector<MorselDesc>& all_morsels() const noexcept { return morsels_; }
        [[nodiscard]] std::size_t remaining_morsels() const noexcept;

    private:
        std::size_t total_rows_{0};
        std::size_t morsel_size_{DEFAULT_MORSEL_SIZE};
        std::vector<MorselDesc> morsels_;
        std::atomic<std::size_t> next_morsel_idx_{0};
    };

} // namespace pqe::storage
