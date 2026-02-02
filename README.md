## Transaction Processing System

A multithreaded transaction processing layer implementing Optimistic Concurrency Control (OCC) and Conservative Two-Phase Locking (2PL).

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

## Database Layer 

The `Database` class provides the following operations:

- `bool Open(const string& db_path)` - Opens or creates a database
- `void Close()` - Closes the database
- `optional<string> Get(const string& key)` - Retrieves a value
- `bool Put(const string& key, const string& value)` - Stores a key-value pair
- `bool Delete(const string& key)` - Deletes a key
- `bool InitializeWithData(const map<string, string>& data)` - Bulk initialization
- `bool Clear()` - Removes all data
- `size_t GetKeyCount()` - Returns number of keys

