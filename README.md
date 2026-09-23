# Parallel Query Execution Engine (PQE) for Analytical Workloads

A high-performance, lightweight analytical database query engine developed in **Modern C++ (C++20)**. The engine addresses memory wall bottlenecks and CPU core underutilization by implementing **intra-operator multi-core CPU parallelism** using **morsel-driven dynamic scheduling** (OpenMP), **zero-copy memory-mapped file ingestion (POSIX mmap / Win32 MapViewOfFile)**, and **cache-friendly contiguous columnar memory layouts (`std::vector<T>`)**.

---

## Directory Architecture

```text
Parallel Query Execution Engine/
├── .gitignore
├── CMakeLists.txt
├── architecture_blueprint.md
├── CHANGELOG.md
├── README.md
│
├── data/
│   └── sample/
│       └── sales_250k.csv
│
├── include/
│   ├── core/
│   │   └── app_config.hpp
│   └── pqe/
│       ├── common/
│       │   └── types.hpp
│       ├── layer1_storage/
│       │   ├── columnar_table.hpp
│       │   ├── mmap_reader.hpp
│       │   └── morsel_allocator.hpp
│       ├── layer3_execution/
│       │   └── sequential/
│       │       ├── seq_aggregator.hpp
│       │       ├── seq_filter.hpp
│       │       ├── seq_group_by.hpp
│       │       └── seq_scan.hpp
│       └── layer5_telemetry/
│           └── telemetry.hpp
│
├── scripts/
│   └── generate_sales_data.py
│
└── src/
    ├── main.cpp
    ├── layer1_storage/
    │   ├── columnar_table.cpp
    │   ├── mmap_reader.cpp
    │   └── morsel_allocator.cpp
    └── layer3_execution/
        └── sequential/
            ├── seq_aggregator.cpp
            ├── seq_filter.cpp
            ├── seq_group_by.cpp
            └── seq_scan.cpp
```

---

## File Component Inventory

| File Path | Core Language/Tech | Purpose & Component Responsibility |
| :--- | :--- | :--- |
| `CMakeLists.txt` | CMake (v3.20+) | Project build manifest orchestrating C++20 standard, compiler warning flags, OpenMP package discovery, static library `pqe_storage`, and runtime executable targets. |
| `architecture_blueprint.md` | Markdown / Mermaid | Technical architecture specification covering the 5-layer design, system topology, microservice breakdown, zero-copy IPC, and development roadmap. |
| `.gitignore` | Git Ignore Rules | Rules preventing build artifacts (`build/`, `bin/`, `lib/`), compiled objects (`*.o`, `*.obj`), precompiled headers, and local IDE metadata from being tracked. |
| `data/sample/sales_250k.csv` | CSV (Structured Data) | Synthetic 250,000-row transactional sales dataset (8.69 MB) adhering to the 7-column OLAP schema for testing and benchmarking. |
| `scripts/generate_sales_data.py` | Python 3 | High-throughput synthetic sales data generator producing 1M–10M+ rows with deterministic seeding, chunked batch writing, and runtime progress statistics. |
| `include/core/app_config.hpp` | C++20 Header | Application-level configuration structures, semantic versioning representation, and runtime environment constants. |
| `include/pqe/common/types.hpp` | C++20 Header | Fundamental engine primitives, type aliases (`row_id_t`, `morsel_id_t`), `MorselDesc` chunk definitions, and structured `SalesRecordView` definitions. |
| `include/pqe/layer1_storage/mmap_reader.hpp` | C++20 Header | RAII abstraction interface for zero-copy memory-mapped file access and kernel cache prefetch advice. |
| `src/layer1_storage/mmap_reader.cpp` | C++20 Source | Cross-platform virtual memory mapping implementation switching between POSIX `mmap()`/`munmap()` on Unix and Win32 `MapViewOfFile()` on Windows. |
| `include/pqe/layer1_storage/columnar_table.hpp` | C++20 Header | Declaration of the in-memory columnar storage buffer storing numeric and string columns in contiguous vectors for L1/L2 cache prefetching. |
| `src/layer1_storage/columnar_table.cpp` | C++20 Source | High-speed, zero-copy CSV parsing implementation converting raw byte buffers into typed columnar arrays using `std::from_chars`. |
| `include/pqe/layer1_storage/morsel_allocator.hpp` | C++20 Header | Interface for the morsel partitioning engine providing thread-safe lock-free atomic chunk dispatching (`100,000` rows/chunk). |
| `src/layer1_storage/morsel_allocator.cpp` | C++20 Source | Implementation of dataset chunking, boundary calculation, and lock-free cursor retrieval (`std::atomic<size_t>::fetch_add`). |
| `include/pqe/layer3_execution/sequential/seq_scan.hpp` | C++20 Header | Single-threaded linear iterator traversing contiguous columnar memory vectors across entire tables or bounded morsels. |
| `src/layer3_execution/sequential/seq_scan.cpp` | C++20 Source | Implementation of the sequential scan iterator and functional `for_each` row traversal. |
| `include/pqe/layer3_execution/sequential/seq_filter.hpp` | C++20 Header | Declarations for predicate filtering operations producing dense row selection vectors (`std::vector<row_id_t>`). |
| `src/layer3_execution/sequential/seq_filter.cpp` | C++20 Source | Optimized linear predicate evaluations for comparison operators (`Quantity > X`, `CategoryID == Y`, conjunctions). |
| `include/pqe/layer3_execution/sequential/seq_aggregator.hpp` | C++20 Header | Single-pass scalar aggregation declarations for `COUNT()`, `SUM()`, `AVG()`, `MIN()`, and `MAX()`. |
| `src/layer3_execution/sequential/seq_aggregator.cpp` | C++20 Source | Implementations of single-pass scalar metric accumulators with cache-conscious contiguous vector sweeps. |
| `include/pqe/layer3_execution/sequential/seq_group_by.hpp` | C++20 Header | Hash-based grouping declarations aggregating running metrics (`COUNT`, `SUM`, `AVG`, `MIN`, `MAX`) keyed on `CategoryID`. |
| `src/layer3_execution/sequential/seq_group_by.cpp` | C++20 Source | Hash aggregation implementation maintaining per-category metrics maps and sorted result rows. |
| `include/pqe/layer5_telemetry/telemetry.hpp` | C++20 Header | High-resolution scoped timer framework using `std::chrono` to record baseline $T_1$ latency and calculate row throughput. |
| `src/main.cpp` | C++20 Source | Application entrypoint orchestrating CLI argument parsing, mmap ingestion, columnar memory loading, baseline sequential query benchmarking ($T_1$), and morsel allocation diagnostics. |

---

## Execution & Deployment Guides

### 1. Environmental Prerequisites & Toolchain Verification

The engine is designed to be cross-platform and requires a modern C++ toolchain supporting **C++20** and **OpenMP**, along with **CMake (>= 3.20)** and **Python (>= 3.10)**. Additionally, for **Intel/AMD CPUs**, the build system automatically detects and enables **AVX2/AVX-512 SIMD vectorization** for accelerated analytical processing.

#### Cross-Platform Installation

**Windows** (via Winget and MSYS2):
```powershell
# Install CMake and Python
winget install Kitware.CMake
winget install Python.Python.3.11

# Install GCC (MinGW-w64) and Make via MSYS2
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make
```

**Ubuntu / Debian Linux** (via apt):
```bash
sudo apt update
sudo apt install build-essential cmake python3 python3-pip libomp-dev
```

**macOS** (via Homebrew):
```bash
brew install cmake python gcc libomp
```

**Verify Toolchain**:
```bash
cmake --version
g++ --version
python --version
```

### Python Dependencies

The data generation and benchmarking scripts require Python. The benchmark script uses `duckdb` to provide a baseline comparison.

```powershell
# Install the required Python packages
pip install duckdb
```

---

### 2. Synthetic Data Generation

Generate test datasets adhering to the 7-column schema (`TransactionID,CustomerID,Quantity,Price,Discount,CategoryID,StoreRegion`):

```powershell
# Generate standard 250,000-row test dataset (approx. ~8.7 MB)
python scripts/generate_sales_data.py --rows 250000 --out data/sample/sales_250k.csv

# Generate 1,000,000-row benchmark dataset (approx. ~35 MB)
python scripts/generate_sales_data.py --rows 1000000 --out data/sample/sales_1M.csv
```

---

### 3. Build & Compilation

Configure and build the project using CMake. The build system will detect OpenMP and compile the static library `pqe_storage` alongside the main engine executable `pqe_engine`. It will also automatically inject `-mavx2` or `-march=native` on Intel/AMD platforms to unlock SIMD vectorization speeds.

**Windows (MinGW Makefiles)**:
```powershell
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Linux / macOS (Unix Makefiles)**:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

*Executable Output*: `build/bin/pqe_engine.exe` (or `build/bin/pqe_engine` on POSIX systems).

---

### 4. Running the Engine

Execute the query engine with configurable dataset paths, morsel chunk sizes, and inspection flags:

```powershell
# Run with default 250k rows dataset and 100,000-row morsels
.\build\bin\pqe_engine.exe --data data/sample/sales_250k.csv --morsel-size 100000 --inspect 5

# Display command-line options and usage parameters
.\build\bin\pqe_engine.exe --help
```

---

### 5. Cleaning Build Artifacts

To remove compiled binaries and intermediate build state:

```powershell
# Remove build directory
Remove-Item -Recurse -Force build
```

---

### 6. Cloning & Replicating the Benchmarks

To clone this repository and replicate the dashboard results, follow these steps:

```powershell
# 1. Clone the repository
git clone <repository_url>
cd "Parallel Query Execution Engine"

# 2. Build the project in Release mode
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 3. Generate a large scale dataset (e.g. 50 Million rows)
python scripts/generate_sales_data.py --rows 50000000 --out data/scale/sales_50m.csv

# 4. Run the benchmark harness to compare against DuckDB
python scripts/run_benchmarks_and_report.py --data data/scale/sales_50m.csv --threads 8
```

This will automatically execute all 10 queries on both PQE and DuckDB, and open an HTML dashboard (`benchmark_dashboard.html`) in your browser visualizing the speedup, execution times, and architecture comparisons.
