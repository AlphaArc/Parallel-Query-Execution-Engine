# Parallel Query Execution Engine (PQE) for Analytical Workloads

A high-performance, lightweight analytical database query engine developed in **Modern C++ (C++20)**. The engine addresses memory wall bottlenecks and CPU core underutilization by implementing **intra-operator multi-core CPU parallelism** using **morsel-driven dynamic scheduling** (OpenMP), **zero-copy memory-mapped file ingestion (POSIX mmap / Win32 MapViewOfFile)**, and **cache-friendly contiguous columnar memory layouts (`std::vector<T>`)**.

---

## Directory Architecture

```text
COA Project/
├── .gitignore
├── CMakeLists.txt
├── architecture_blueprint.md
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
│       └── layer1_storage/
│           ├── columnar_table.hpp
│           ├── mmap_reader.hpp
│           └── morsel_allocator.hpp
│
├── scripts/
│   └── generate_sales_data.py
│
└── src/
    ├── main.cpp
    └── layer1_storage/
        ├── columnar_table.cpp
        ├── mmap_reader.cpp
        └── morsel_allocator.cpp
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
| `src/main.cpp` | C++20 Source | Application entrypoint orchestrating CLI argument parsing, mmap ingestion, columnar memory loading, performance timing (`std::chrono`), and morsel allocation diagnostics. |

---

## Execution & Deployment Guides

### 1. Environmental Prerequisites & Toolchain Verification

Ensure a modern C++ toolchain supporting **C++20** and **OpenMP** is available, along with **CMake (>= 3.20)** and **Python (>= 3.10)**:

```powershell
# Verify CMake installation
cmake --version

# Verify C++ Compiler (GCC / MinGW-w64 or Clang)
g++ --version

# Verify Python installation
python --version
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

Configure and build the project using CMake. The build system will detect OpenMP and compile the static library `pqe_storage` alongside the main engine executable `coa_engine`:

```powershell
# Configure build directory (MinGW Makefiles on Windows)
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Compile all targets
cmake --build build --config Release
```

*Executable Output*: `build/bin/coa_engine.exe` (or `build/bin/coa_engine` on POSIX systems).

---

### 4. Running the Engine

Execute the query engine with configurable dataset paths, morsel chunk sizes, and inspection flags:

```powershell
# Run with default 250k rows dataset and 100,000-row morsels
.\build\bin\coa_engine.exe --data data/sample/sales_250k.csv --morsel-size 100000 --inspect 5

# Display command-line options and usage parameters
.\build\bin\coa_engine.exe --help
```

---

### 5. Cleaning Build Artifacts

To remove compiled binaries and intermediate build state:

```powershell
# Remove build directory
Remove-Item -Recurse -Force build
```
