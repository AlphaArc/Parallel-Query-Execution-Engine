cmake_minimum_required(VERSION 3.20)

project(
    PQEProject
    VERSION 0.1.0
    DESCRIPTION "Parallel Query Execution Engine for Analytical Workloads"
    LANGUAGES CXX
)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# OpenMP Support
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found: Enabling multi-core parallelism")
else()
    message(WARNING "OpenMP not found: Proceeding with serial CPU execution only")
endif()

# Compiler Warnings Configuration
set(COA_WARNING_FLAGS "")
if(MSVC)
    set(COA_WARNING_FLAGS /W4 /permissive-)
else()
    set(COA_WARNING_FLAGS 
        -Wall 
        -Wextra 
        -Wpedantic 
        -Wshadow 
        -Wunused
    )
endif()

# ==============================================================================
# Layer 1: Storage Engine Library
# ==============================================================================
add_library(pqe_storage STATIC
    src/layer1_storage/mmap_reader.cpp
    src/layer1_storage/columnar_table.cpp
    src/layer1_storage/morsel_allocator.cpp
)

target_include_directories(pqe_storage
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(pqe_storage PRIVATE ${COA_WARNING_FLAGS})

if(OpenMP_CXX_FOUND)
    target_link_libraries(pqe_storage PUBLIC OpenMP::OpenMP_CXX)
endif()

# ==============================================================================
# Layer 2/3/5: Query Execution & Telemetry Library
# ==============================================================================
add_library(pqe_core STATIC
    src/layer3_execution/sequential/seq_scan.cpp
    src/layer3_execution/sequential/seq_filter.cpp
    src/layer3_execution/sequential/seq_aggregator.cpp
    src/layer3_execution/sequential/seq_group_by.cpp
    src/layer3_execution/parallel/omp_scan.cpp
    src/layer3_execution/parallel/omp_filter.cpp
    src/layer3_execution/parallel/omp_aggregator.cpp
    src/layer3_execution/parallel/omp_group_by.cpp
)

target_include_directories(pqe_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(pqe_core PRIVATE ${COA_WARNING_FLAGS})
target_link_libraries(pqe_core PUBLIC pqe_storage)

if(OpenMP_CXX_FOUND)
    target_link_libraries(pqe_core PUBLIC OpenMP::OpenMP_CXX)
endif()

# ==============================================================================
# Engine Executable & Verification CLI
# ==============================================================================
add_executable(pqe_engine
    src/main.cpp
)

target_include_directories(pqe_engine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(pqe_engine PRIVATE ${COA_WARNING_FLAGS})
target_link_libraries(pqe_engine PRIVATE pqe_core pqe_storage)

if(OpenMP_CXX_FOUND)
    target_link_libraries(pqe_engine PRIVATE OpenMP::OpenMP_CXX)
endif()
