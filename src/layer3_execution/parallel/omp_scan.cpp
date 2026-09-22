#include "pqe/layer3_execution/parallel/omp_scan.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"

#include <omp.h>

namespace pqe::execution::parallel {

    OmpScan::OmpScan(const storage::ColumnarTable& table) noexcept
        : table_(table) {}

    void OmpScan::for_each(const std::function<void(row_id_t)>& callback) const {
        storage::MorselAllocator allocator(table_.row_count(), pqe::DEFAULT_MORSEL_SIZE);

        #pragma omp parallel
        {
            while (auto morsel_opt = allocator.get_next_morsel()) {
                const auto& morsel = *morsel_opt;
                for (row_id_t r = morsel.start_row; r < morsel.end_row; ++r) {
                    callback(r);
                }
            }
        }
    }

} // namespace pqe::execution::parallel
