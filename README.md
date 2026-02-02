# Transaction Processing System

A multithreaded transaction processing layer implementing Optimistic Concurrency Control (OCC) and Conservative Two-Phase Locking (2PL).

## Current Status: Step 1 - Database Layer ✓

The database layer is implemented using RocksDB as the underlying key-value store.

## Prerequisites

- C++ compiler with C++17 support (GCC 7+, Clang 5+, or MSVC 2017+)
- CMake 3.15 or higher
- RocksDB library

### Installing RocksDB

**macOS (using Homebrew):**
```bash
brew install rocksdb
```

**Ubuntu/Debian:**
```bash
sudo apt-get install librocksdb-dev
```

**From source:**
```bash
git clone https://github.com/facebook/rocksdb.git
cd rocksdb
make shared_lib
sudo make install
```

## Build Instructions

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
make

# Run tests
./test_database

# Run main program
./transaction_system
```

## Project Structure

```
223Project/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── src/
│   ├── database/              # Database layer (Step 1)
│   │   ├── database.h
│   │   └── database.cpp
│   ├── transaction/           # Transaction layer (Step 2 - TODO)
│   ├── concurrency/           # OCC and 2PL (Step 4 - TODO)
│   └── main.cpp               # Main program
├── workloads/                 # Workload definitions (Step 5 - TODO)
└── tests/                     # Unit tests
    └── test_database.cpp
```

## Database Layer API

The `Database` class provides the following operations:

- `bool Open(const string& db_path)` - Opens or creates a database
- `void Close()` - Closes the database
- `optional<string> Get(const string& key)` - Retrieves a value
- `bool Put(const string& key, const string& value)` - Stores a key-value pair
- `bool Delete(const string& key)` - Deletes a key
- `bool InitializeWithData(const map<string, string>& data)` - Bulk initialization
- `bool Clear()` - Removes all data
- `size_t GetKeyCount()` - Returns number of keys

## Next Steps

- [ ] Step 2: Implement transaction layer (begin, read, write, commit)
- [ ] Step 3: Build terminal UI for user commands
- [ ] Step 4: Implement OCC and Conservative 2PL protocols
- [ ] Step 5: Create workload definitions and execution engine
- [ ] Step 6: Add performance measurement and statistics
- [ ] Step 7: Generate performance graphs and report

## Notes

- RocksDB is thread-safe for concurrent reads and writes
- The database layer stores values as strings; structured objects can be serialized (e.g., as JSON)
- All data persists to disk in the specified database directory
