# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### [Added]
- Documentation: Created `README.md` with complete architecture layout, file component inventory, and execution/deployment guide.
- Documentation: Created `CHANGELOG.md` following Keep a Changelog standards.

---

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
