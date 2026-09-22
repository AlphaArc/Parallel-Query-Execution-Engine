# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### [Added]
- **Documentation & Benchmarking Enhancements**:
  - Enhanced `benchmark_dashboard.html` template in `scripts/run_benchmarks_and_report.py` to capture and display execution metadata (OS, Timestamps, Python/DuckDB versions).
  - Added explicit architectural explanations in the HTML dashboard detailing where DuckDB wins (Q4, Q8, Q10 Hash Group By operations utilizing AVX-512) vs PQE (Q1, Q2, Q3 raw scanning).
  - Updated `README.md` with explicit instructions for cloning the repository, building in Release mode, generating scaled datasets, and running the benchmarking harness.
  - Added `duckdb` Python package to the `README.md` dependencies list.
- **Repository Maintenance**:
  - Updated `.gitignore` to safely exclude generated large-scale datasets (`data/scale/` and `*.csv`).
  - Removed redundant `scripts/benchmark_duckdb.py` standalone script, as its functionality is fully subsumed by the comprehensive `scripts/run_benchmarks_and_report.py` harness.
- **Hardware AVX2 SIMD Vectorization**:
  - Implemented manual AVX2 hardware intrinsics (`_mm256_loadu_ps`, `_mm256_add_ps`, `_mm256_i32gather_ps`, etc.) in `OmpAggregator` for massive arithmetic speedups (`aggregate_price_simd` and `sum_net_sales_simd`).
  - Added `-mavx2` / `/arch:AVX2` compiler flags and horizontal vector reduction helpers.
  - Python Dashboard now displays a dedicated **PQE Latency (SIMD AVX)** column, comparing the SIMD latency against standard scalar Parallel latency and DuckDB.
- **Extended 10-Query Benchmark Suite**:
  - Expanded `main.cpp` sequential and parallel profiling to execute 6 new complex analytical queries (`Q5`-`Q10`), heavily utilizing existing `OmpFilter`, `OmpAggregator` and pipelining structures.
  - Upgraded Python Benchmark Dashboard to parse all 10 queries, adding HTML hover tooltips (revealing SQL equivalents) and dynamically displaying the first 5 raw dataset records.
- **Cardinality-Aware Group By & Automation (Phase 6)**:
  - `OmpGroupBy` now dynamically selects Lock-Free Thread-Local maps for low-cardinality keys (`CategoryID`), completely eliminating the lock contention bottleneck.
  - Developed unified `scripts/run_benchmarks_and_report.py` to automatically execute the C++ engine, run DuckDB baselines, and generate/open a Tailwind CSS HTML comparison dashboard.
- **Phase 4 Concurrency & Synchronization (Layer 4)**:
  - `SpinLock` (`include/pqe/layer4_concurrency/spin_lock.hpp`): Ultra-low latency user-space spinlock utilizing atomic test-and-set to avoid OS context switches.
  - `AtomicAccumulator` (`include/pqe/layer4_concurrency/atomic_accumulator.hpp`): Lock-free CAS floating-point addition wrapper.
  - `ConcurrentShardedMap` (`include/pqe/layer4_concurrency/concurrent_hash_map.hpp`): High-performance lock-free sharded hash map for scalable parallel Group By operations, padded to avoid false sharing.
  - `SPSCRingBuffer` (`include/pqe/layer4_concurrency/ring_buffer.hpp`): Lock-free single-producer single-consumer ring buffer for IPC.
- **Phase 5 Advanced Telemetry (Layer 5)**:
  - `CycleCounter` (`include/pqe/layer5_telemetry/cycle_counter.hpp`): Exact CPU cycle profiling via x86 `__rdtsc()` intrinsic.
  - Parallel Speedup Profiling (`include/pqe/layer5_telemetry/speedup_calculator.hpp`): Real-time output of Speedup ($S_N$) and Efficiency metrics.
- **Phase 3 Parallel Execution Engine (Layer 3)**:
  - OpenMP implementations (`OmpScan`, `OmpFilter`, `OmpAggregator`, `OmpGroupBy`) executing across parallel morsels.
  - Replaced thread-local hash maps and OpenMP atomic directives with Phase 4 lock-free concurrency structures.
- **Phase 2 Baseline Sequential Execution Engine (Layer 3)**:
  - `SeqScan` (`include/pqe/layer3_execution/sequential/seq_scan.hpp`, `src/layer3_execution/sequential/seq_scan.cpp`): Single-threaded iterator linearly traversing contiguous columnar memory vectors across entire tables or bounded morsels.
  - `SeqFilter` (`include/pqe/layer3_execution/sequential/seq_filter.hpp`, `src/layer3_execution/sequential/seq_filter.cpp`): Predicate evaluation engine emitting dense row selection vectors (`std::vector<row_id_t>`) for downstream zero-copy pipelining.
  - `SeqAggregator` (`include/pqe/layer3_execution/sequential/seq_aggregator.hpp`, `src/layer3_execution/sequential/seq_aggregator.cpp`): Single-pass scalar aggregations for `COUNT()`, `SUM()`, `AVG()`, `MIN()`, and `MAX()`.
  - `SeqGroupBy` (`include/pqe/layer3_execution/sequential/seq_group_by.hpp`, `src/layer3_execution/sequential/seq_group_by.cpp`): Single-threaded hash table aggregating running metrics (`COUNT`, `SUM(Quantity)`, `AVG(Price)`, `MIN(Price)`, `MAX(Price)`) partitioned by `CategoryID`.
- **Baseline Telemetry & Timing Framework (Layer 5)**:
  - `ScopedTimer` and `QueryMetrics` (`include/pqe/layer5_telemetry/telemetry.hpp`): High-resolution `std::chrono` timers capturing execution time $T_1$, row processing latency ($\mu s / ms$), and throughput (rows/sec).
- **Benchmark Analytical Query Suite**:
  - Extended `src/main.cpp` with an end-to-end evaluation suite running 4 benchmark queries (Full Scan, Predicate Filter, Multi-Metric Aggregation, and Hash GROUP BY) establishing the $T_1$ baseline.
- **Build System Extension**:
  - Added `pqe_engine` static library target to `CMakeLists.txt` linked with `pqe_storage` and OpenMP.

---

## [0.1.1] - 2026-09-17 00:33:43 +0530

### [Added]
- [`fdd45b3`] (2026-09-17 00:33:43 +0530) **Project Documentation**: Created comprehensive `README.md` containing architectural overview, ASCII directory tree, file component inventory table, and execution/deployment instructions.
- [`fdd45b3`] (2026-09-17 00:33:43 +0530) **Changelog**: Initialized `CHANGELOG.md` following Keep a Changelog standards.

## [0.1.0] - 2026-09-16 22:28:45 +0530

### [Added]
- [`a7d6f53`] (2026-09-16 22:28:45 +0530) **Core Project Scaffolding**: Initialized modern C++20 build system with target-based `CMakeLists.txt` supporting OpenMP 5.2 and strict compiler warning flags (`-Wall -Wextra -Wpedantic` / `/W4 /permissive-`).
- [`a7d6f53`] (2026-09-16 22:28:45 +0530) **Architecture Blueprint**: Authored `architecture_blueprint.md` specifying the 5-layer query engine design, Mermaid system topology, microservice breakdown, and lock-free zero-copy IPC model.
- [`a7d6f53`] (2026-09-16 22:28:45 +0530) **Data Generator**: Created `scripts/generate_sales_data.py` producing synthetic OLAP sales records across the 7-column schema (`TransactionID`, `CustomerID`, `Quantity`, `Price`, `Discount`, `CategoryID`, `StoreRegion`) with buffered chunk writing.
- [`a7d6f53`] (2026-09-16 22:28:45 +0530) **Memory-Mapped Storage Layer (Layer 1)**:
  - Implemented cross-platform `MmapReader` (`include/pqe/layer1_storage/mmap_reader.hpp`, `src/layer1_storage/mmap_reader.cpp`) supporting POSIX `mmap()`/`munmap()` and Win32 `CreateFileMappingA()`/`MapViewOfFile()` with sequential kernel prefetch advice.
  - Implemented `ColumnarTable` (`include/pqe/layer1_storage/columnar_table.hpp`, `src/layer1_storage/columnar_table.cpp`) storing table data in contiguous native `std::vector<T>` columns (`int32_t`, `float`, `std::string`) with fast zero-copy `std::from_chars` CSV parsing.
- [`a7d6f53`] (2026-09-16 22:28:45 +0530) **Morsel Chunk Allocator**: Built `MorselAllocator` (`include/pqe/layer1_storage/morsel_allocator.hpp`, `src/layer1_storage/morsel_allocator.cpp`) partitioning columnar data into non-overlapping 100,000-row chunks with atomic, lock-free `fetch_add` dispatching.
- [`a7d6f53`] (2026-09-16 22:28:45 +0530) **Engine CLI & Supervisor Entrypoint**: Developed `src/main.cpp` providing interactive command-line ingestion, performance benchmarking timers (`std::chrono`), and morsel partition inspection.
- [`a7d6f53`] (2026-09-16 22:28:45 +0530) **Test Dataset**: Generated sample 250,000-row benchmark dataset `data/sample/sales_250k.csv` (8.69 MB).
- [`a7d6f53`] (2026-09-16 22:28:45 +0530) **Version & Primitives**: Defined global application configuration in `include/core/app_config.hpp` and engine types in `include/pqe/common/types.hpp`.
- [`a7d6f53`] (2026-09-16 22:28:45 +0530) **Repository Standards**: Configured `.gitignore` for C++, CMake, build outputs, and IDE metadata.
