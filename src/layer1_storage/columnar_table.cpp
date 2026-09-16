#include "pqe/layer1_storage/columnar_table.hpp"

#include <charconv>
#include <iostream>
#include <string_view>
#include <stdexcept>

namespace pqe::storage {

    void ColumnarTable::reserve(std::size_t capacity) {
        transaction_id_.reserve(capacity);
        customer_id_.reserve(capacity);
        quantity_.reserve(capacity);
        price_.reserve(capacity);
        discount_.reserve(capacity);
        category_id_.reserve(capacity);
        store_region_.reserve(capacity);
    }

    void ColumnarTable::clear() noexcept {
        transaction_id_.clear();
        customer_id_.clear();
        quantity_.clear();
        price_.clear();
        discount_.clear();
        category_id_.clear();
        store_region_.clear();
    }

    void ColumnarTable::push_back(std::int32_t tx_id, std::int32_t cust_id, std::int32_t qty,
                                  float price, float discount, std::int32_t cat_id,
                                  std::string_view region) {
        transaction_id_.push_back(tx_id);
        customer_id_.push_back(cust_id);
        quantity_.push_back(qty);
        price_.push_back(price);
        discount_.push_back(discount);
        category_id_.push_back(cat_id);
        store_region_.emplace_back(region);
    }

    SalesRecordView ColumnarTable::get_row(row_id_t row_idx) const {
        if (row_idx >= row_count()) {
            throw std::out_of_range("Row index out of range in ColumnarTable");
        }
        return SalesRecordView{
            transaction_id_[row_idx],
            customer_id_[row_idx],
            quantity_[row_idx],
            price_[row_idx],
            discount_[row_idx],
            category_id_[row_idx],
            store_region_[row_idx]
        };
    }

    std::size_t ColumnarTable::memory_usage_bytes() const noexcept {
        std::size_t total = 0;
        total += transaction_id_.capacity() * sizeof(std::int32_t);
        total += customer_id_.capacity() * sizeof(std::int32_t);
        total += quantity_.capacity() * sizeof(std::int32_t);
        total += price_.capacity() * sizeof(float);
        total += discount_.capacity() * sizeof(float);
        total += category_id_.capacity() * sizeof(std::int32_t);
        total += store_region_.capacity() * sizeof(std::string);
        for (const auto& s : store_region_) {
            total += s.capacity();
        }
        return total;
    }

    namespace {
        // Fast helper to parse integer via std::from_chars
        inline bool parse_int(const char* start, const char* end, std::int32_t& out) {
            auto [ptr, ec] = std::from_chars(start, end, out);
            return ec == std::errc();
        }

        // Fast helper to parse float via std::from_chars
        inline bool parse_float(const char* start, const char* end, float& out) {
            auto [ptr, ec] = std::from_chars(start, end, out);
            return ec == std::errc();
        }
    }

    bool ColumnarTable::load_from_mmap(const MmapReader& reader) {
        clear();
        if (!reader.is_open() || reader.size() == 0) {
            return false;
        }

        const char* ptr = reader.data();
        const char* end = ptr + reader.size();

        // Heuristic: ~55 bytes per CSV record for sales dataset
        const std::size_t estimated_rows = reader.size() / 55 + 1000;
        reserve(estimated_rows);

        // Skip header line
        while (ptr < end && *ptr != '\n') {
            ++ptr;
        }
        if (ptr < end && *ptr == '\n') {
            ++ptr;
        }

        // Fast zero-copy line and token scanning
        while (ptr < end) {
            if (*ptr == '\r' || *ptr == '\n') {
                ++ptr;
                continue;
            }

            const char* line_start = ptr;
            while (ptr < end && *ptr != '\n') {
                ++ptr;
            }
            const char* line_end = ptr;
            if (line_end > line_start && *(line_end - 1) == '\r') {
                --line_end;
            }
            if (ptr < end) {
                ++ptr; // Move past '\n'
            }

            // Parse 7 comma-separated fields:
            // TransactionID,CustomerID,Quantity,Price,Discount,CategoryID,StoreRegion
            const char* cursor = line_start;
            const char* token_start = cursor;
            int field_index = 0;

            std::int32_t tx_id = 0;
            std::int32_t cust_id = 0;
            std::int32_t qty = 0;
            float price = 0.0f;
            float discount = 0.0f;
            std::int32_t cat_id = 0;
            std::string_view region{};

            while (cursor <= line_end) {
                if (cursor == line_end || *cursor == ',') {
                    switch (field_index) {
                        case 0: parse_int(token_start, cursor, tx_id); break;
                        case 1: parse_int(token_start, cursor, cust_id); break;
                        case 2: parse_int(token_start, cursor, qty); break;
                        case 3: parse_float(token_start, cursor, price); break;
                        case 4: parse_float(token_start, cursor, discount); break;
                        case 5: parse_int(token_start, cursor, cat_id); break;
                        case 6: region = std::string_view(token_start, static_cast<std::size_t>(cursor - token_start)); break;
                        default: break;
                    }
                    ++field_index;
                    token_start = cursor + 1;
                }
                ++cursor;
            }

            if (field_index >= 7) {
                push_back(tx_id, cust_id, qty, price, discount, cat_id, region);
            }
        }

        return true;
    }

} // namespace pqe::storage
