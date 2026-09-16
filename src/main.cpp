#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/mmap_reader.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"
#include "pqe/layer3_execution/sequential/seq_scan.hpp"
#include "pqe/layer3_execution/sequential/seq_filter.hpp"
#include "pqe/layer3_execution/sequential/seq_aggregator.hpp"
#include "pqe/layer3_execution/sequential/seq_group_by.hpp"
#include "pqe/layer5_telemetry/telemetry.hpp"

#include <iostream>
#include <iomanip>
#include <string_view>
#include <vector>
#include <chrono>

namespace {
    void print_banner() {
        std::cout << "===============================================================\n";
        std::cout << "  Parallel Query Execution Engine (PQE) - Analytical Core\n";
        std::cout << "  Phase 2: Baseline Sequential Execution Engine (T1 Reference)\n";
        std::cout << "===============================================================\n";
    }

    void print_help(std::string_view exec_name) {
        std::cout << "Usage: " << exec_name << " [options]\n\n"
                  << "Options:\n"
                  << "  -h, --help                Show this help message\n"
                  << "  -d, --data <file.csv>     Path to sales dataset CSV\n"
                  << "  -m, --morsel-size <rows>  Rows per morsel chunk (default: 100000)\n"
                  << "  -i, --inspect <count>     Inspect first N rows of columnar table (default: 5)\n"
                  << "  --run-queries             Run full baseline sequential analytical query suite\n";
    }

    void run_sequential_queries(const pqe::storage::ColumnarTable& table) {
        std::cout << "\n===============================================================\n";
        std::cout << "  EXECUTING BASELINE SEQUENTIAL QUERIES (T1 PROFILING)\n";
        std::cout << "===============================================================\n";

        const std::size_t total_rows = table.row_count();
        pqe::execution::sequential::SeqScan scan(table);
        pqe::execution::sequential::SeqFilter filter(table);
        pqe::execution::sequential::SeqAggregator agg(table);
        pqe::execution::sequential::SeqGroupBy group_by(table);

        // -------------------------------------------------------------
        // Query 1: Full Table Scan & Row Count
        // SQL: SELECT COUNT(*) FROM sales;
        // -------------------------------------------------------------
        {
            pqe::telemetry::ScopedTimer timer("Q1: Full Table Scan & COUNT(*)", total_rows);
            std::size_t count = agg.count();
            auto metrics = timer.stop(count);
            metrics.print_report();
            std::cout << "  -> Result: " << count << " rows counted.\n\n";
        }

        // -------------------------------------------------------------
        // Query 2: Predicate Filter Selection
        // SQL: SELECT * FROM sales WHERE Quantity > 50 AND CategoryID == 10;
        // -------------------------------------------------------------
        {
            pqe::telemetry::ScopedTimer timer("Q2: Filter (Quantity > 50 AND CategoryID == 10)", total_rows);
            auto selection = filter.filter_quantity_and_category(50, 10);
            auto metrics = timer.stop(selection.size());
            metrics.print_report();
            double selectivity = (static_cast<double>(selection.size()) / static_cast<double>(total_rows)) * 100.0;
            std::cout << "  -> Result: " << selection.size() << " matched rows (" 
                      << std::fixed << std::setprecision(2) << selectivity << "% selectivity)\n\n";
        }

        // -------------------------------------------------------------
        // Query 3: Filter + Multi-Metric Aggregation
        // SQL: SELECT COUNT(*), SUM(Price), AVG(Price), MIN(Price), MAX(Price) 
        //      FROM sales WHERE Quantity > 50;
        // -------------------------------------------------------------
        {
            pqe::telemetry::ScopedTimer timer("Q3: Filter (Quantity > 50) + Aggregation (COUNT, SUM, AVG, MIN, MAX)", total_rows);
            auto selection = filter.filter_quantity_gt(50);
            auto result = agg.aggregate_price(selection);
            auto metrics = timer.stop(result.count);
            metrics.print_report();
            result.print_report("Price WHERE Quantity > 50");
            std::cout << "\n";
        }

        // -------------------------------------------------------------
        // Query 4: Hash GROUP BY Aggregation
        // SQL: SELECT CategoryID, COUNT(*), SUM(Quantity), AVG(Price), MIN(Price), MAX(Price) 
        //      FROM sales GROUP BY CategoryID ORDER BY CategoryID;
        // -------------------------------------------------------------
        {
            pqe::telemetry::ScopedTimer timer("Q4: Hash GROUP BY CategoryID (Aggregations)", total_rows);
            auto grouped_rows = group_by.group_by_category_sorted();
            auto metrics = timer.stop(total_rows);
            metrics.print_report();

            std::cout << "  -> Total Groups Created: " << grouped_rows.size() << "\n";
            std::cout << "  -> Preview First 5 Grouped Categories:\n";
            std::cout << "     " 
                      << std::left
                      << std::setw(8)  << "CatID"
                      << std::setw(12) << "Count"
                      << std::setw(14) << "SumQty"
                      << std::setw(14) << "AvgPrice"
                      << std::setw(14) << "MinPrice"
                      << std::setw(14) << "MaxPrice" << "\n";
            std::cout << "     " << std::string(72, '-') << "\n";

            for (std::size_t i = 0; i < std::min(std::size_t{5}, grouped_rows.size()); ++i) {
                const auto& g = grouped_rows[i];
                std::cout << "     "
                          << std::left
                          << std::setw(8)  << g.category_id
                          << std::setw(12) << g.metrics.count
                          << std::setw(14) << g.metrics.sum_quantity
                          << std::setw(14) << std::fixed << std::setprecision(2) << g.metrics.avg_price()
                          << std::setw(14) << std::fixed << std::setprecision(2) << g.metrics.min_price
                          << std::setw(14) << std::fixed << std::setprecision(2) << g.metrics.max_price << "\n";
            }
            std::cout << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    print_banner();

    std::string data_path = "data/sample/sales_250k.csv";
    std::size_t morsel_size = pqe::DEFAULT_MORSEL_SIZE;
    std::size_t inspect_count = 5;
    bool run_queries = true;

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
        } else if (args[i] == "--no-queries") {
            run_queries = false;
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

    // Step 4: Morsel Partitioning Diagnostics
    std::cout << "---------------------------------------------------------------\n";
    std::cout << "[STAGE 3] Partitioning dataset into 100k morsels...\n";
    pqe::storage::MorselAllocator allocator(table, morsel_size);

    std::cout << "  -> Total morsels created: " << allocator.morsel_count() << "\n";
    for (const auto& morsel : allocator.all_morsels()) {
        std::cout << "     Morsel #" << morsel.morsel_id 
                  << " : Rows [" << morsel.start_row << ", " << morsel.end_row << ")"
                  << " (" << morsel.row_count() << " rows)\n";
    }

    // Step 5: Always run or conditionally run sequential query suite
    if (run_queries) {
        run_sequential_queries(table);
    }

    std::cout << "===============================================================\n";
    std::cout << "[STATUS] Phase 2 Sequential Engine verified successfully!\n";
    std::cout << "===============================================================\n";

    return 0;
}
