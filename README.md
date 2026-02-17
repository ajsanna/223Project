## Transaction Processing System

A multi-threaded transaction processing layer built on RocksDB, implementing Optimistic Concurrency Control (OCC) with support for configurable contention and workload execution. Built for CS 223 Winter 2026.

## Dependencies

- **CMake** >= 3.15
- **C++17** compatible compiler
- **RocksDB** (installed via Homebrew: `brew install rocksdb`)

## Build Instructions

```bash
mkdir build && cd build
cmake ..
make
```

This produces three executables in `build/`:
- `transaction_system` — main workload runner
- `test_database` — database layer tests
- `test_occ` — OCC concurrency control tests

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
| `--hotset-prob P` | Probability of selecting a hot key (0.0-1.0) | 0.5 |
| `--protocol P` | Concurrency protocol (`occ`) | occ |
| `--db-path PATH` | RocksDB directory path | transaction_db |

### Examples

```bash
# Low contention, single thread
./transaction_system --threads 1 --txns-per-thread 500 --hotset-prob 0.0

# High contention, 8 threads
./transaction_system --threads 8 --txns-per-thread 250 --hotset-size 5 --hotset-prob 1.0

# Custom database path
./transaction_system --db-path /tmp/my_test_db
```

## Running Tests

```bash
cd build

# Database layer tests
./test_database

# OCC concurrency control tests
./test_occ
```

### OCC Test Coverage

The `test_occ` suite covers:

**Transaction struct (Phase 1):**
- Read-your-writes semantics
- Read from DB populates read_set
- Write buffering with last-write-wins

**OCC validation (Phase 2):**
- Single transaction commit + DB write-through
- Read-only transaction commit
- Sequential transactions without conflict
- Conflict detection (concurrent write to read key)
- No conflict on disjoint key sets
- Abort clears read/write sets, leaves DB unchanged
- Timestamp monotonicity across commits

**Multi-threaded correctness (Phase 3):**
- Zero aborts with partitioned (non-overlapping) keys
- Balance conservation under concurrent transfers (4 threads, 200 txns each)
- High contention (3 hot keys, 4 threads) produces aborts while preserving invariants

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
│   │   ├── transaction.h           # Transaction struct (read/write sets, timestamps)
│   │   └── transaction.cpp         # Read (read-your-writes) and Write helpers
│   ├── concurrency/
│   │   ├── transaction_manager.h   # Abstract CC protocol interface
│   │   ├── occ_manager.h           # OCC implementation header
│   │   └── occ_manager.cpp         # OCC validation logic
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
    └── test_occ.cpp                # OCC concurrency control tests
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

### Workload Execution
- Configurable hotset-based key selection (contention level)
- Three transaction templates: transfer (zero-sum), balance_check (read-only), write_heavy (n increments)
- Multi-threaded executor with per-thread random template selection
- Response time measured from first Begin to successful Commit (includes retries)

### Metrics
- Per-type commit/abort counters
- Latency recording with avg, P50, P90, P99 percentiles
- Throughput calculation
- Formatted report output

## What's Not Yet Implemented

- **Conservative 2PL** — lock manager, all-or-nothing acquisition, livelock prevention
- **Structured values** — values are currently plain strings, not maps/dictionaries
- **Workload input parsing** — workloads are currently defined in code, not parsed from text files
- **Experiment scripts and graphs** — automated benchmarking and plot generation
