## Transaction Processing System

A multi-threaded transaction processing layer built on RocksDB, implementing **Optimistic Concurrency Control (OCC)** and **Conservative Two-Phase Locking (2PL)** with configurable contention and workload execution. Built for CS 223 Winter 2026.

## Dependencies

- **CMake** >= 3.15
- **C++20** compatible compiler (MinGW on Windows, clang/gcc on macOS/Linux)
- **RocksDB** (via [vcpkg](https://vcpkg.io/) on Windows, or `brew install rocksdb` on macOS)

## Build Instructions

### Windows (MinGW + vcpkg)

```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
```

### macOS / Linux

```bash
mkdir build && cd build
cmake ..
make
```

This produces the following executables in `build/`:
- `transaction_system` — main workload runner
- `test_database` — database layer tests
- `test_occ` — OCC concurrency control tests
- `test_2pl` — Conservative 2PL tests

## Running the System

```bash
./transaction_system [options]
```

### CLI Options

| Flag | Description | Default |
|------|-------------|---------|
| `--threads N` | Number of worker threads | 4 |
| `--txns-per-thread N` | Transactions per thread | 100 |
| `--total-keys N` | Total keys in the database | 1000 |
| `--hotset-size N` | Number of hot keys | 10 |
| `--hotset-prob P` | Probability of selecting a hot key (0.0–1.0) | 0.5 |
| `--protocol P` | Concurrency protocol: `occ` or `2pl` | occ |
| `--db-path PATH` | RocksDB directory path | transaction_db |

### Examples

```bash
# OCC, low contention, 4 threads
./transaction_system --protocol occ --threads 4 --txns-per-thread 500 --hotset-prob 0.1

# 2PL, high contention, 8 threads
./transaction_system --protocol 2pl --threads 8 --txns-per-thread 250 --hotset-size 5 --hotset-prob 0.9

# OCC vs 2PL comparison at fixed contention
./transaction_system --protocol occ  --threads 4 --hotset-prob 0.5
./transaction_system --protocol 2pl  --threads 4 --hotset-prob 0.5
```

## Running Tests

```bash
cd build

# Database layer tests
./test_database

# OCC concurrency control tests (13 tests)
./test_occ

# Conservative 2PL tests (12 tests)
./test_2pl
```

On Windows, use the provided PowerShell scripts:

```powershell
powershell -ExecutionPolicy Bypass -File build/run_test2.ps1      # OCC
powershell -ExecutionPolicy Bypass -File build/run_test_2pl.ps1   # 2PL
```

### Test Coverage

**OCC (`test_occ`) — 13 tests:**

*Transaction struct (Phase 1):*
- Read-your-writes semantics
- Read from DB populates read_set
- Write buffering with last-write-wins

*OCC validation (Phase 2):*
- Single transaction commit + DB write-through
- Read-only transaction commit
- Sequential transactions without conflict
- Conflict detection (concurrent write to read key)
- No conflict on disjoint key sets
- Abort clears read/write sets, leaves DB unchanged
- Timestamp monotonicity across commits

*Multi-threaded correctness (Phase 3):*
- Zero aborts with partitioned (non-overlapping) keys
- Balance conservation under concurrent transfers (4 threads, 200 txns each)
- High contention (3 hot keys, 4 threads) produces aborts while preserving invariants

**Conservative 2PL (`test_2pl`) — 12 tests:**

*LockManager unit tests (Phase 1):*
- TryAcquireAll succeeds when all keys are free
- TryAcquireAll fails (returns false, acquires nothing) when any key is held
- ReleaseAll frees keys so next TryAcquireAll succeeds
- All-or-nothing: no partial lock state is ever left behind

*TwoPLManager single-threaded (Phase 2):*
- Basic begin/read/write/commit flow
- Read-your-writes: sees own buffered writes before commit
- Commit always returns success=true
- retry_count is 0 when no contention

*Multi-threaded correctness (Phase 3):*
- Partitioned keys: zero lock retries, no waiting
- Balance conservation under concurrent transfers (all 800 transactions commit)
- High contention: all transactions eventually commit, balance preserved
- CommitResult.success is always true (unlike OCC)

## Project Structure

```
223Project/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── database/
│   │   ├── database.h              # RocksDB wrapper interface
│   │   └── database.cpp
│   ├── transaction/
│   │   ├── transaction.h           # Transaction struct (read/write sets, lock_keys, timestamps)
│   │   └── transaction.cpp         # Read (read-your-writes) and Write helpers
│   ├── concurrency/
│   │   ├── transaction_manager.h   # Abstract CC protocol interface
│   │   ├── occ_manager.h           # OCC implementation header
│   │   ├── occ_manager.cpp         # OCC validation logic
│   │   ├── twopl_manager.h         # Conservative 2PL + LockManager header
│   │   └── twopl_manager.cpp       # Lock acquisition, backoff, commit/abort
│   ├── workload/
│   │   ├── key_selector.h          # Hotset-based key selection (header-only)
│   │   ├── workload_template.h     # Transaction templates (transfer, balance_check, write_heavy)
│   │   ├── workload_executor.h     # Multi-threaded executor header
│   │   └── workload_executor.cpp   # Thread pool, retry loop, metrics collection
│   ├── metrics/
│   │   ├── metrics.h               # MetricsCollector header
│   │   └── metrics.cpp             # Aggregation, percentiles, reporting
│   └── main.cpp                    # CLI entry point
└── tests/
    ├── test_database.cpp           # Database layer tests
    ├── test_occ.cpp                # OCC concurrency control tests
    └── test_2pl.cpp                # Conservative 2PL tests
```

## What's Implemented

### Storage Layer
- RocksDB wrapper with Get/Put/Delete/Clear/InitializeWithData

### OCC (Optimistic Concurrency Control)
- Transaction struct with read_set, write_set, timestamps, and status
- Read-your-writes semantics (reads check write buffer first)
- Writes buffered privately until commit
- Sequential validation under a global mutex
- Conflict detection: committed transaction's write_keys intersected with active transaction's read_set
- Monotonic timestamp assignment (start_ts, validation_ts, finish_ts)
- Committed history tracking with garbage collection support
- Retry with exponential backoff + jitter on abort

### Conservative 2PL (Two-Phase Locking)
- LockManager with exclusive-only locking for all accesses
- All-or-nothing lock acquisition: TryAcquireAll either locks every key atomically or acquires nothing
- Locks acquired upfront in Begin() before any reads/writes execute (conservative phase)
- Locks released on Commit or Abort (shrinking phase)
- Livelock prevention via exponential backoff with per-thread random jitter
- Commit always succeeds — no validation failures possible

### Workload Execution
- Configurable hotset-based key selection (contention level)
- Three transaction templates: transfer (zero-sum), balance_check (read-only), write_heavy (n increments)
- Multi-threaded executor with per-thread random template selection
- Response time measured from first Begin to successful Commit (includes retries/backoff)

### Metrics
- Per-type commit/abort counters and abort percentage
- Latency recording with avg, P50, P90, P99 percentiles
- Throughput calculation (committed transactions per second)
- Formatted report output
