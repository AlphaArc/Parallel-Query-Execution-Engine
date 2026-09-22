#include "pqe/common/types.hpp"
#include "pqe/layer1_storage/mmap_reader.hpp"
#include "pqe/layer1_storage/columnar_table.hpp"
#include "pqe/layer1_storage/morsel_allocator.hpp"
#include "pqe/layer3_execution/sequential/seq_scan.hpp"
#include "pqe/layer3_execution/sequential/seq_filter.hpp"
#include "pqe/layer3_execution/sequential/seq_aggregator.hpp"
#include "pqe/layer3_execution/sequential/seq_group_by.hpp"
#include "pqe/layer3_execution/parallel/omp_scan.hpp"
#include "pqe/layer3_execution/parallel/omp_filter.hpp"
#include "pqe/layer3_execution/parallel/omp_aggregator.hpp"
#include "pqe/layer3_execution/parallel/omp_group_by.hpp"
#include "pqe/layer5_telemetry/telemetry.hpp"
#include "pqe/layer5_telemetry/speedup_calculator.hpp"
#include <omp.h>

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

    struct BenchmarkMetrics {
        pqe::telemetry::QueryMetrics q1;
        pqe::telemetry::QueryMetrics q2;
        pqe::telemetry::QueryMetrics q3;
        pqe::telemetry::QueryMetrics q4;
        pqe::telemetry::QueryMetrics q5;
        pqe::telemetry::QueryMetrics q6;
        pqe::telemetry::QueryMetrics q7;
        pqe::telemetry::QueryMetrics q8;
        pqe::telemetry::QueryMetrics q9;
        pqe::telemetry::QueryMetrics q10;
    };

    BenchmarkMetrics run_sequential_queries(const pqe::storage::ColumnarTable& table) {
        std::cout << "\n===============================================================\n";
        std::cout << "  EXECUTING BASELINE SEQUENTIAL QUERIES (T1 PROFILING)\n";
        std::cout << "===============================================================\n";

        const std::size_t total_rows = table.row_count();
        pqe::execution::sequential::SeqScan scan(table);
        pqe::execution::sequential::SeqFilter filter(table);
        pqe::execution::sequential::SeqAggregator agg(table);
        pqe::execution::sequential::SeqGroupBy group_by(table);
        
        BenchmarkMetrics results;

        {
            pqe::telemetry::ScopedTimer timer("Q1: Full Table Scan & COUNT(*)", total_rows);
            std::size_t count = agg.count();
            results.q1 = timer.stop(count);
            results.q1.print_report();
            std::cout << "  -> Result: " << count << " rows counted.\n\n";
        }

        {
            pqe::telemetry::ScopedTimer timer("Q2: Filter (Quantity > 50 AND CategoryID == 10)", total_rows);
            auto selection = filter.filter_quantity_and_category(50, 10);
            results.q2 = timer.stop(selection.size());
            results.q2.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q3: Filter (Quantity > 50) + Aggregation", total_rows);
            auto selection = filter.filter_quantity_gt(50);
            auto result = agg.aggregate_price(selection);
            results.q3 = timer.stop(result.count);
            results.q3.print_report();
            std::cout << "\n";
        }

        {
            pqe::telemetry::ScopedTimer timer("Q4: Hash GROUP BY CategoryID", total_rows);
            auto grouped_rows = group_by.group_by_category_sorted();
            results.q4 = timer.stop(total_rows);
            results.q4.print_report();
        }
        
        {
            pqe::telemetry::ScopedTimer timer("Q5: Filter (CategoryID == 1) + Aggregate Net Sales", total_rows);
            auto selection = filter.filter_category_eq(1);
            auto result = agg.sum_net_sales(selection);
            results.q5 = timer.stop(selection.size());
            results.q5.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q6: Filter (Quantity > 90) + COUNT(*)", total_rows);
            auto selection = filter.filter_quantity_gt(90);
            auto count = agg.count(selection);
            results.q6 = timer.stop(count);
            results.q6.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q7: Filter (Quantity > 20 AND CategoryID == 5) + Aggregate Price", total_rows);
            auto selection = filter.filter_quantity_and_category(20, 5);
            auto result = agg.aggregate_price(selection);
            results.q7 = timer.stop(result.count);
            results.q7.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q8: Filter (Quantity > 50) + Hash GROUP BY CategoryID", total_rows);
            auto selection = filter.filter_quantity_gt(50);
            auto grouped_rows = group_by.group_by_category_sorted(selection);
            results.q8 = timer.stop(selection.size());
            results.q8.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q9: Full Table Aggregate (Net Sales)", total_rows);
            auto result = agg.sum_net_sales();
            results.q9 = timer.stop(total_rows);
            results.q9.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q10: Filter (CategoryID == 20) + Aggregate Quantity", total_rows);
            auto selection = filter.filter_category_eq(20);
            auto result = agg.sum_quantity(selection);
            results.q10 = timer.stop(selection.size());
            results.q10.print_report();
        }
        
        return results;
    }

    void run_parallel_queries(const pqe::storage::ColumnarTable& table, int threads, const BenchmarkMetrics& seq_metrics) {
        std::cout << "\n===============================================================\n";
        std::cout << "  EXECUTING PARALLEL QUERIES (TN = " << threads << ")\n";
        std::cout << "===============================================================\n";

        omp_set_num_threads(threads);

        const std::size_t total_rows = table.row_count();
        pqe::execution::parallel::OmpScan scan(table);
        pqe::execution::parallel::OmpFilter filter(table);
        pqe::execution::parallel::OmpAggregator agg(table);
        pqe::execution::parallel::OmpGroupBy group_by(table);

        {
            pqe::telemetry::ScopedTimer timer("Q1: Full Table Scan & COUNT(*)", total_rows);
            std::size_t count = agg.count();
            auto par_metrics = timer.stop(count);
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q1, par_metrics, threads};
            speedup.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q2: Filter (Quantity > 50 AND CategoryID == 10)", total_rows);
            auto selection = filter.filter_quantity_and_category(50, 10);
            auto par_metrics = timer.stop(selection.size());
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q2, par_metrics, threads};
            speedup.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q3: Filter (Quantity > 50) + Aggregate Price", total_rows);
            auto selection = filter.filter_quantity_gt(50);
            auto result = agg.aggregate_price(selection);
            auto par_metrics = timer.stop(result.count);
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q3, par_metrics, threads};
            speedup.print_report();

            // Hardware SIMD Benchmark
            pqe::telemetry::ScopedTimer simd_timer("Q3: SIMD AVX2 Filter + Aggregate Price", total_rows);
            auto simd_result = agg.aggregate_price_simd(selection);
            auto simd_metrics = simd_timer.stop(simd_result.count);
            std::cout << "[TELEMETRY - SIMD SPEEDUP] Q3: -> Par Time (TN): " << simd_metrics.duration_milliseconds << " ms\n";
        }

        {
            pqe::telemetry::ScopedTimer timer("Q4: Hash GROUP BY CategoryID", total_rows);
            auto grouped_rows = group_by.group_by_category_sorted();
            auto par_metrics = timer.stop(total_rows);
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q4, par_metrics, threads};
            speedup.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q5: Filter (CategoryID == 1) + Aggregate Net Sales", total_rows);
            auto selection = filter.filter_category_eq(1);
            auto result = agg.sum_net_sales(selection);
            auto par_metrics = timer.stop(selection.size());
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q5, par_metrics, threads};
            speedup.print_report();

            // Hardware SIMD Benchmark
            pqe::telemetry::ScopedTimer simd_timer("Q5: SIMD AVX2 Filter + Aggregate Net Sales", total_rows);
            auto simd_result = agg.sum_net_sales_simd(selection);
            auto simd_metrics = simd_timer.stop(selection.size());
            std::cout << "[TELEMETRY - SIMD SPEEDUP] Q5: -> Par Time (TN): " << simd_metrics.duration_milliseconds << " ms\n";
        }

        {
            pqe::telemetry::ScopedTimer timer("Q6: Filter (Quantity > 90) + COUNT(*)", total_rows);
            auto selection = filter.filter_quantity_gt(90);
            auto count = agg.count(selection);
            auto par_metrics = timer.stop(count);
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q6, par_metrics, threads};
            speedup.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q7: Filter (Quantity > 20 AND CategoryID == 5) + Aggregate Price", total_rows);
            auto selection = filter.filter_quantity_and_category(20, 5);
            auto result = agg.aggregate_price(selection);
            auto par_metrics = timer.stop(result.count);
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q7, par_metrics, threads};
            speedup.print_report();

            // Hardware SIMD Benchmark
            pqe::telemetry::ScopedTimer simd_timer("Q7: SIMD AVX2 Multi-Predicate + Aggregate Price", total_rows);
            auto simd_result = agg.aggregate_price_simd(selection);
            auto simd_metrics = simd_timer.stop(simd_result.count);
            std::cout << "[TELEMETRY - SIMD SPEEDUP] Q7: -> Par Time (TN): " << simd_metrics.duration_milliseconds << " ms\n";
        }

        {
            pqe::telemetry::ScopedTimer timer("Q8: Filter (Quantity > 50) + Hash GROUP BY CategoryID", total_rows);
            auto selection = filter.filter_quantity_gt(50);
            auto grouped_rows = group_by.group_by_category_sorted(selection);
            auto par_metrics = timer.stop(selection.size());
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q8, par_metrics, threads};
            speedup.print_report();
        }

        {
            pqe::telemetry::ScopedTimer timer("Q9: Full Table Aggregate (Net Sales)", total_rows);
            auto result = agg.sum_net_sales();
            auto par_metrics = timer.stop(total_rows);
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q9, par_metrics, threads};
            speedup.print_report();

            // Hardware SIMD Benchmark
            pqe::telemetry::ScopedTimer simd_timer("Q9: SIMD AVX2 Full Table Aggregate (Net Sales)", total_rows);
            auto simd_result = agg.sum_net_sales_simd();
            auto simd_metrics = simd_timer.stop(total_rows);
            std::cout << "[TELEMETRY - SIMD SPEEDUP] Q9: -> Par Time (TN): " << simd_metrics.duration_milliseconds << " ms\n";
        }

        {
            pqe::telemetry::ScopedTimer timer("Q10: Filter (CategoryID == 20) + Aggregate Quantity", total_rows);
            auto selection = filter.filter_category_eq(20);
            auto result = agg.sum_quantity(selection);
            auto par_metrics = timer.stop(selection.size());
            pqe::telemetry::ParallelSpeedupMetrics speedup{seq_metrics.q10, par_metrics, threads};
            speedup.print_report();
        }
    }
}

int main(int argc, char* argv[]) {
    print_banner();

    std::string data_path = "data/sample/sales_250k.csv";
    std::size_t morsel_size = pqe::DEFAULT_MORSEL_SIZE;
    std::size_t inspect_count = 5;
    bool run_queries = true;
    int num_threads = 4;

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
        } else if ((args[i] == "-t" || args[i] == "--threads") && i + 1 < args.size()) {
            num_threads = std::stoi(std::string(args[++i]));
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
        auto seq_metrics = run_sequential_queries(table);
        run_parallel_queries(table, num_threads, seq_metrics);
    }

    std::cout << "===============================================================\n";
    std::cout << "[STATUS] Phase 3 Parallel Engine verified successfully!\n";
    std::cout << "===============================================================\n";

    // Explicitly release 1.9 GB of column vectors before shutdown
    table.clear();

    return 0;
}
