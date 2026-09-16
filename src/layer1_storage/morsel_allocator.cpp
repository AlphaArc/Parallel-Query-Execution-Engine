#include "pqe/layer1_storage/morsel_allocator.hpp"

#include <algorithm>

namespace pqe::storage {

    MorselAllocator::MorselAllocator(std::size_t morsel_size) noexcept
        : morsel_size_(morsel_size == 0 ? DEFAULT_MORSEL_SIZE : morsel_size) {}

    MorselAllocator::MorselAllocator(const ColumnarTable& table, std::size_t morsel_size)
        : morsel_size_(morsel_size == 0 ? DEFAULT_MORSEL_SIZE : morsel_size) {
        partition(table.row_count(), morsel_size_);
    }

    MorselAllocator::MorselAllocator(std::size_t total_rows, std::size_t morsel_size)
        : morsel_size_(morsel_size == 0 ? DEFAULT_MORSEL_SIZE : morsel_size) {
        partition(total_rows, morsel_size_);
    }

    void MorselAllocator::partition(std::size_t total_rows, std::size_t morsel_size) {
        total_rows_ = total_rows;
        morsel_size_ = (morsel_size == 0) ? DEFAULT_MORSEL_SIZE : morsel_size;
        morsels_.clear();

        if (total_rows_ == 0) {
            next_morsel_idx_.store(0, std::memory_order_relaxed);
            return;
        }

        const std::size_t count = (total_rows_ + morsel_size_ - 1) / morsel_size_;
        morsels_.reserve(count);

        for (std::size_t i = 0; i < count; ++i) {
            row_id_t start = i * morsel_size_;
            row_id_t end = std::min(start + morsel_size_, total_rows_);
            morsels_.push_back(MorselDesc{
                .morsel_id = i,
                .start_row = start,
                .end_row = end
            });
        }

        next_morsel_idx_.store(0, std::memory_order_relaxed);
    }

    void MorselAllocator::reset() noexcept {
        next_morsel_idx_.store(0, std::memory_order_release);
    }

    std::optional<MorselDesc> MorselAllocator::get_next_morsel() noexcept {
        const std::size_t idx = next_morsel_idx_.fetch_add(1, std::memory_order_acq_rel);
        if (idx < morsels_.size()) {
            return morsels_[idx];
        }
        return std::nullopt;
    }

    std::size_t MorselAllocator::remaining_morsels() const noexcept {
        const std::size_t current = next_morsel_idx_.load(std::memory_order_acquire);
        if (current < morsels_.size()) {
            return morsels_.size() - current;
        }
        return 0;
    }

} // namespace pqe::storage
