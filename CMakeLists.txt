cmake_minimum_required(VERSION 3.15)
project(TransactionProcessing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find RocksDB
find_package(RocksDB REQUIRED)

# Find Threads
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Database layer
add_library(database
    src/database/database.cpp
)
target_link_libraries(database RocksDB::rocksdb)

# Transaction layer
add_library(transaction
    src/transaction/transaction.cpp
)
target_link_libraries(transaction database)

# Concurrency layer
add_library(concurrency
    src/concurrency/occ_manager.cpp
)
target_link_libraries(concurrency transaction database)

# Metrics layer
add_library(metrics
    src/metrics/metrics.cpp
)

# Workload layer
add_library(workload
    src/workload/workload_executor.cpp
)
target_link_libraries(workload concurrency metrics Threads::Threads)

# Test executable for database layer
add_executable(test_database
    tests/test_database.cpp
)
target_link_libraries(test_database database)

# Main executable
add_executable(transaction_system
    src/main.cpp
)
target_link_libraries(transaction_system workload concurrency metrics transaction database Threads::Threads)

# Test executable for OCC
add_executable(test_occ
    tests/test_occ.cpp
)
target_link_libraries(test_occ concurrency transaction database Threads::Threads)
