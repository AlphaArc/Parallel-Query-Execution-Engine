#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <string>

namespace pqe {

    using row_id_t = std::size_t;
    using morsel_id_t = std::size_t;

    inline constexpr std::size_t DEFAULT_MORSEL_SIZE = 100'000;

    struct MorselDesc {
        morsel_id_t morsel_id{0};
        row_id_t start_row{0};
        row_id_t end_row{0};

        [[nodiscard]] constexpr std::size_t row_count() const noexcept {
            return (end_row >= start_row) ? (end_row - start_row) : 0;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return start_row >= end_row;
        }
    };

    struct SalesRecordView {
        std::int32_t transaction_id;
        std::int32_t customer_id;
        std::int32_t quantity;
        float price;
        float discount;
        std::int32_t category_id;
        std::string_view store_region;
    };

} // namespace pqe
