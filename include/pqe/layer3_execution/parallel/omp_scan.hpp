#pragma once

#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"

#include <cstddef>
#include <functional>

namespace pqe::execution::parallel {

    class OmpScan {
    public:
        explicit OmpScan(const storage::ColumnarTable& table) noexcept;

        // Functional scan: executes a callback per row in parallel across threads.
        // NOTE: callback must be thread-safe!
        void for_each(const std::function<void(row_id_t)>& callback) const;

    private:
        const storage::ColumnarTable& table_;
    };

} // namespace pqe::execution::parallel
