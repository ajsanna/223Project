cmake_minimum_required(VERSION 3.15)
project(TransactionProcessing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find RocksDB
find_package(RocksDB REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Database layer
add_library(database
    src/database/database.cpp
)
target_link_libraries(database RocksDB::rocksdb)

# Test executable for database layer
add_executable(test_database
    tests/test_database.cpp
)
target_link_libraries(test_database database)

# Main executable (will be used later for terminal UI)
add_executable(transaction_system
    src/main.cpp
)
target_link_libraries(transaction_system database)
