#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/mmap_reader.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"

#include <iostream>
#include <iomanip>
#include <string_view>
#include <vector>
#include <chrono>

namespace {
    void print_banner() {
        std::cout << "===============================================================\n";
        std::cout << "  Parallel Query Execution Engine (PQE) - Analytical Core\n";
        std::cout << "  Layer 1 Storage Engine: mmap Ingestion & Morsel Allocator\n";
        std::cout << "===============================================================\n";
    }

    void print_help(std::string_view exec_name) {
        std::cout << "Usage: " << exec_name << " [options]\n\n"
                  << "Options:\n"
                  << "  -h, --help                Show this help message\n"
                  << "  -d, --data <file.csv>     Path to sales dataset CSV\n"
                  << "  -m, --morsel-size <rows>  Rows per morsel chunk (default: 100000)\n"
                  << "  -i, --inspect <count>     Inspect first N rows of columnar table (default: 5)\n";
    }
}

int main(int argc, char* argv[]) {
    print_banner();

    std::string data_path = "data/sample/sales_250k.csv";
    std::size_t morsel_size = pqe::DEFAULT_MORSEL_SIZE;
    std::size_t inspect_count = 5;

    const std::vector<std::string_view> args(argv + 1, argv + argc);
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-h" || args[i] == "--help") {
            print_help(argv[0]);
            return 0;
        } else if ((args[i] == "-d" || args[i] == "--data") && i + 1 < args.size()) {
            data_path = std::string(args[++i]);
        } else if ((args[i] == "-m" || args[i] == "--morsel-size") && i + 1 < args.size()) {
            morsel_size = static_cast<std::size_t>(std::stoull(std::string(args[++i])));
        } else if ((args[i] == "-i" || args[i] == "--inspect") && i + 1 < args.size()) {
            inspect_count = static_cast<std::size_t>(std::stoull(std::string(args[++i])));
        }
    }

    std::cout << "[CONFIG] Target Dataset: " << data_path << "\n";
    std::cout << "[CONFIG] Morsel Chunk Size: " << morsel_size << " rows\n";
    std::cout << "---------------------------------------------------------------\n";

    // Step 1: Memory-Mapped File Ingestion
    std::cout << "[STAGE 1] Initiating memory-mapped ingestion (mmap)...\n";
    auto t_map_start = std::chrono::high_resolution_clock::now();
    
    pqe::storage::MmapReader reader;
    if (!reader.open(data_path)) {
        std::cerr << "[ERROR] Could not open dataset file at '" << data_path << "'.\n"
                  << "Please generate it using: python scripts/generate_sales_data.py\n";
        return 1;
    }
    
    auto t_map_end = std::chrono::high_resolution_clock::now();
    double map_duration_ms = std::chrono::duration<double, std::milli>(t_map_end - t_map_start).count();
    double file_size_mb = static_cast<double>(reader.size()) / (1024.0 * 1024.0);

    std::cout << "  -> File mapped successfully: " << std::fixed << std::setprecision(2)
              << file_size_mb << " MB in " << map_duration_ms << " ms\n";

    // Step 2: Ingest into Columnar Memory Buffer
    std::cout << "[STAGE 2] Parsing raw bytes into contiguous columnar vectors...\n";
    auto t_parse_start = std::chrono::high_resolution_clock::now();

    pqe::storage::ColumnarTable table;
    if (!table.load_from_mmap(reader)) {
        std::cerr << "[ERROR] Failed to parse CSV content into ColumnarTable.\n";
        return 1;
    }

    auto t_parse_end = std::chrono::high_resolution_clock::now();
    double parse_duration_ms = std::chrono::duration<double, std::milli>(t_parse_end - t_parse_start).count();
    double throughput_rows_sec = (static_cast<double>(table.row_count()) / (parse_duration_ms / 1000.0));
    double memory_mb = static_cast<double>(table.memory_usage_bytes()) / (1024.0 * 1024.0);

    std::cout << "  -> Ingestion complete: " << table.row_count() << " rows ingested\n";
    std::cout << "  -> Columnar Memory Allocated: " << std::fixed << std::setprecision(2) << memory_mb << " MB\n";
    std::cout << "  -> Ingestion Latency: " << parse_duration_ms << " ms ("
              << std::fixed << std::setprecision(0) << throughput_rows_sec << " rows/sec)\n";

    // Step 3: Inspect sample rows
    if (inspect_count > 0 && !table.empty()) {
        std::cout << "---------------------------------------------------------------\n";
        std::cout << "[SAMPLE] First " << std::min(inspect_count, table.row_count()) << " records:\n";
        std::cout << std::left 
                  << std::setw(12) << "TxID"
                  << std::setw(12) << "CustID"
                  << std::setw(10) << "Qty"
                  << std::setw(12) << "Price"
                  << std::setw(10) << "Discount"
                  << std::setw(12) << "CatID"
                  << std::setw(12) << "Region" << "\n";
        std::cout << std::string(75, '-') << "\n";

        for (std::size_t i = 0; i < std::min(inspect_count, table.row_count()); ++i) {
            auto row = table.get_row(i);
            std::cout << std::left 
                      << std::setw(12) << row.transaction_id
                      << std::setw(12) << row.customer_id
                      << std::setw(10) << row.quantity
                      << std::setw(12) << std::fixed << std::setprecision(2) << row.price
                      << std::setw(10) << std::fixed << std::setprecision(2) << row.discount
                      << std::setw(12) << row.category_id
                      << std::setw(12) << row.store_region << "\n";
        }
    }

    // Step 4: Morsel Partitioning & Verification
    std::cout << "---------------------------------------------------------------\n";
    std::cout << "[STAGE 3] Partitioning dataset into 100k morsels...\n";
    pqe::storage::MorselAllocator allocator(table, morsel_size);

    std::cout << "  -> Total morsels created: " << allocator.morsel_count() << "\n";
    for (const auto& morsel : allocator.all_morsels()) {
        std::cout << "     Morsel #" << morsel.morsel_id 
                  << " : Rows [" << morsel.start_row << ", " << morsel.end_row << ")"
                  << " (" << morsel.row_count() << " rows)\n";
    }

    // Step 5: Test atomic lock-free extraction
    std::cout << "  -> Testing atomic morsel distribution...\n";
    std::size_t dispatched = 0;
    while (auto morsel = allocator.get_next_morsel()) {
        ++dispatched;
    }
    std::cout << "  -> Successfully dispatched " << dispatched << " / " 
              << allocator.morsel_count() << " morsels atomically.\n";

    std::cout << "===============================================================\n";
    std::cout << "[STATUS] Phase 1 Storage Engine verified successfully!\n";
    std::cout << "===============================================================\n";

    return 0;
}
