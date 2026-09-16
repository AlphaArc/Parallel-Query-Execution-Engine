#pragma once

#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/mmap_reader.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <span>

namespace pqe::storage {

    class ColumnarTable {
    public:
        ColumnarTable() = default;

        // Reserves memory capacity across all column vectors to eliminate reallocations
        void reserve(std::size_t capacity);

        // Clears all stored columns
        void clear() noexcept;

        // Loads and converts CSV data from memory-mapped bytes into contiguous column vectors
        bool load_from_mmap(const MmapReader& reader);

        // Appends a single record (useful for testing and streaming ingestion)
        void push_back(std::int32_t tx_id, std::int32_t cust_id, std::int32_t qty,
                       float price, float discount, std::int32_t cat_id,
                       std::string_view region);

        // Accessors
        [[nodiscard]] std::size_t row_count() const noexcept { return transaction_id_.size(); }
        [[nodiscard]] bool empty() const noexcept { return transaction_id_.empty(); }

        // Raw contiguous column access (for SIMD vectorization and cache locality)
        [[nodiscard]] std::span<const std::int32_t> transaction_ids() const noexcept { return transaction_id_; }
        [[nodiscard]] std::span<const std::int32_t> customer_ids() const noexcept { return customer_id_; }
        [[nodiscard]] std::span<const std::int32_t> quantities() const noexcept { return quantity_; }
        [[nodiscard]] std::span<const float> prices() const noexcept { return price_; }
        [[nodiscard]] std::span<const float> discounts() const noexcept { return discount_; }
        [[nodiscard]] std::span<const std::int32_t> category_ids() const noexcept { return category_id_; }
        [[nodiscard]] const std::vector<std::string>& store_regions() const noexcept { return store_region_; }

        // Returns a view of a single row
        [[nodiscard]] SalesRecordView get_row(row_id_t row_idx) const;

        // Calculates approximate in-memory footprint in bytes
        [[nodiscard]] std::size_t memory_usage_bytes() const noexcept;

    private:
        // Contiguous column vectors
        std::vector<std::int32_t> transaction_id_;
        std::vector<std::int32_t> customer_id_;
        std::vector<std::int32_t> quantity_;
        std::vector<float> price_;
        std::vector<float> discount_;
        std::vector<std::int32_t> category_id_;
        std::vector<std::string> store_region_;
    };

} // namespace pqe::storage
