# Transaction Processing System

Multi-threaded transaction processing layer built on RocksDB for CS 223 (Winter 2026). Implements two concurrency control protocols — **Optimistic Concurrency Control (OCC)** and **Conservative Two-Phase Locking (2PL)** — and benchmarks them against two structured workloads under varying contention and thread counts.

---

## Quick Start

```bash
# Build
./txn build

# Run a single experiment
./txn run --workload 1 --protocol occ --threads 4 --txns 200

# Full benchmark sweep (~10 min, 100 runs)
./txn bench

# Generate graphs
./txn plot
```

---

## Dependencies

- CMake >= 3.15
- C++20 compiler (clang or gcc)
- RocksDB — `brew install rocksdb` on macOS
- Python 3 with `pandas` and `matplotlib` (only needed for `./txn plot`)

---

## CLI Reference

Everything goes through the `./txn` script in the project root.

```
./txn <command> [options]
```

### Commands

| Command | Description |
|---------|-------------|
| `build` | Configure CMake and compile |
| `run` | Run one experiment |
| `bench` | Run the full 100-run parameter sweep |
| `plot` | Generate PNG graphs from collected results |
| `clean` | Delete `build/` and temp databases |
| `help` | Print usage |

### `./txn run` options

| Flag | Description | Default |
|------|-------------|---------|
| `--workload 1\|2` | Which workload to run | `1` |
| `--protocol occ\|2pl` | Concurrency protocol | `occ` |
| `--threads N` | Worker threads | `4` |
| `--txns N` | Transactions per thread | `100` |
| `--hotset-size N` | Number of hot keys | `10` |
| `--hotset-prob P` | Probability of picking a hot key (0.0–1.0) | `0.5` |
| `--csv PATH` | Append a metrics row to a CSV file | — |
| `--latencies PATH` | Dump raw latency samples to CSV | — |
| `--db-path PATH` | Override the RocksDB directory | auto |

The `--db-path` defaults to `db_w{workload}_{protocol}` if not specified. Running the same workload/protocol combination twice will reuse the same DB; delete it or use `--db-path` to start fresh.

### Examples

```bash
# Low contention, OCC
./txn run --workload 1 --protocol occ --threads 8 --txns 500 --hotset-prob 0.1

# High contention, 2PL
./txn run --workload 1 --protocol 2pl --threads 8 --txns 500 --hotset-prob 0.9

# Workload 2, save results
./txn run --workload 2 --protocol occ --threads 4 --txns 200 \
    --csv results/my_results.csv \
    --latencies results/my_latencies.csv

# Run the full sweep, then plot
./txn bench
./txn plot
```

---

## Running Tests

```bash
./build/test_database
./build/test_occ
./build/test_2pl
```

---

## Project Structure

```
223Project/
├── txn                             # CLI entry point
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── database/
│   │   ├── database.h              # RocksDB wrapper
│   │   └── database.cpp
│   ├── transaction/
│   │   ├── transaction.h           # Transaction struct (read/write sets, timestamps)
│   │   └── transaction.cpp
│   ├── concurrency/
│   │   ├── transaction_manager.h   # Abstract interface both protocols implement
│   │   ├── occ_manager.h / .cpp    # OCC: buffered writes, timestamp validation
│   │   ├── twopl_manager.h / .cpp  # Conservative 2PL: upfront locking, no aborts
│   ├── workload/
│   │   ├── key_selector.h          # Hotset key selection + MultiDomainKeySelector
│   │   ├── record.h / .cpp         # Structured field storage (serialize/deserialize)
│   │   ├── input_parser.h / .cpp   # Parses workloads/*/input*.txt
│   │   ├── workload_template.h     # WorkloadTemplate struct
│   │   ├── workload1_templates.h   # W1: transfer
│   │   ├── workload2_templates.h   # W2: new_order, payment
│   │   ├── workload_executor.h / .cpp
│   ├── metrics/
│   │   ├── metrics.h / .cpp        # Counters, latency, percentiles, CSV output
├── workloads/
│   ├── workload1/input1.txt        # 500 A_* account records
│   └── workload2/input2.txt        # 8 W + 80 D + 800 S + ~8100 C records
├── scripts/
│   ├── run_experiments.sh          # 100-run parameter sweep
│   └── plot_results.py             # Generates 12 PNGs in results/plots/
└── tests/
    ├── test_database.cpp
    ├── test_occ.cpp
    └── test_2pl.cpp
```

---

## How It Works

### Storage

The database layer wraps RocksDB — an LSM-tree-based embedded key-value store. All values are stored as serialized strings. For workloads with structured records (multi-field rows like `{balance: 153, name: "Account-1"}`), values are serialized as pipe-delimited `field=value` pairs sorted by field name, e.g. `balance=153|name=Account-1`. This is handled by `record.h` and lets the transaction logic read, modify individual fields, and write back without a schema layer.

### Transaction Interface

Both protocols implement the same `TransactionManager` interface:

```
Begin(name, keys) → Transaction
Read(txn, key)    → optional<string>
Write(txn, key, value)
Commit(txn)       → CommitResult { success, retry_count }
Abort(txn)
```

The `keys` passed to `Begin` are the full key set the transaction will touch. OCC uses this for logging; Conservative 2PL uses it to acquire all locks upfront.

---

## Optimistic Concurrency Control (OCC)

OCC is based on the Kung & Robinson (1981) model. The premise is that conflicts are rare, so you don't pay for locking on every access — you just detect conflicts at commit time and retry if one occurred.

**Three phases per transaction:**

**1. Read phase** — the transaction executes speculatively. Reads go directly to the database (no locks taken), but the key is recorded in a read set. Writes are buffered in a private write set and not flushed to the database yet. Read-your-writes is implemented: if a key is in the write buffer, reads return the buffered value instead of hitting the database.

**2. Validation phase** — when the transaction calls `Commit()`, it enters validation under a global mutex. The validator checks the committed transaction history for any transaction that:
- committed after our transaction's `start_ts`, and
- wrote a key that our transaction read

If any such transaction exists, there's a read-write conflict — our transaction read a value that was subsequently overwritten by a transaction that has already committed, meaning our read set is stale. The transaction is aborted.

**3. Write phase** — if validation passes, the write set is flushed to RocksDB, timestamps are advanced, and the transaction record is added to the committed history. The mutex is released.

**Timestamps** are monotonically increasing integers. Each transaction gets a `start_ts` on `Begin()`. On successful commit it receives a `commit_ts`. The validator looks at all committed transactions with `commit_ts > start_ts` of the validating transaction.

**Retry logic** lives in `workload_executor.cpp`. On abort, the thread waits for an exponential backoff interval with random jitter, then re-executes the entire transaction from scratch (re-reads, re-computes, re-validates). Latency is measured from the first `Begin()` to the final successful `Commit()`, so retry costs are included.

**Behavior under contention:** abort rate rises sharply as hotset probability increases because more transactions are reading the same hot keys, and any committed write to a hot key invalidates all concurrent readers. Under very high contention with many threads, OCC can thrash — every transaction aborts the others — so throughput collapses even though no thread is blocked. This is the key tradeoff vs. 2PL.

---

## Conservative Two-Phase Locking (2PL)

Standard 2PL allows a transaction to acquire locks as it goes (growing phase) and release them only after its last access (shrinking phase). The classic problem is deadlock — two transactions can each hold a lock the other needs, waiting forever.

**Conservative 2PL** solves this by requiring a transaction to declare all the keys it will touch upfront and acquire all of them atomically before executing. If it can't get every key, it gets none and retries. Because no partial lock state is ever left behind, deadlock is structurally impossible — a cycle can't form if you either hold everything or nothing.

**Lock acquisition in `Begin()`:** `TryAcquireAll` iterates the lock table under a global mutex. If any key in the requested set is already locked, it immediately releases any keys it may have locked in this attempt and returns false. The caller backs off and retries. This is the all-or-nothing property.

**Execution phase:** once all locks are held, the transaction reads and writes freely. Since no other transaction can hold any of our keys (we checked atomically), reads go straight to the database and writes go to a private buffer exactly like OCC.

**Commit:** flush the write buffer to RocksDB, release all locks. Commit never fails — there's no validation step because the locks prevent any conflicting concurrent write from happening during our execution.

**Behavior under contention:** instead of aborts, you get lock waiting. Threads stall in the `Begin()` retry loop until the keys they need are released. This increases latency (especially at the tail — P99 can get very high) but preserves throughput better than OCC under sustained high contention because no work is thrown away. The tradeoff is that under low contention, OCC is faster because there's no lock acquisition overhead at all.

---

## Workloads

Both workloads are loaded from structured input files in `workloads/`. The parser reads `KEY: X, VALUE: {field: val, ...}` records and stores them as serialized `Record` strings.

### Workload 1 — Bank Transfers

500 accounts (`A_1` through `A_500`), each with a `name` and `balance` field. The only transaction type is a transfer: decrement `balance` by 1 on one account, increment by 1 on another. This is zero-sum — the sum of all balances must be identical before and after every run, which is verified automatically.

Contention is controlled by the hotset: the first `hotset_size` accounts are "hot" and are selected with probability `hotset_prob`. The remaining accounts share probability `1 - hotset_prob`. A hotset of 10 with `hotset_prob = 0.9` means 90% of key accesses go to 10 of the 500 accounts.

### Workload 2 — TPC-C-like

Modeled after the TPC-C benchmark. The database has four entity types:

- **8 Warehouses** (`W_1` – `W_8`): track year-to-date revenue (`ytd`)
- **80 Districts** (`D_w_d`): 10 per warehouse, track `next_o_id` and `ytd`
- **800 Supply items** (`S_w_i`): 100 per warehouse, track `qty`, `ytd`, `order_cnt`
- **~8100 Customers** (`C_w_d_c`): 10 per district, track `balance`, `ytd_payment`, `payment_cnt`

Two transaction types:

**new_order** — touches 4 keys: one district and three supply items. Increments `next_o_id` on the district; decrements `qty` and increments `ytd`/`order_cnt` on each supply item. Models an incoming customer order.

**payment** — touches 3 keys: one warehouse, one district, one customer. Adds 5 to `ytd` on the warehouse and district; subtracts 5 from customer `balance`, increments `ytd_payment` and `payment_cnt`. Models a customer payment.

Hotset scaling is proportional across domains. Since workload 1 has 500 keys and workload 2's domains have sizes 8/80/800/8100, `hotset_size` is scaled per domain as `max(1, domain_size * hotset_size / 500)` so the `--hotset-size` flag is comparable across both workloads.

---

## Benchmarking

### Parameter Matrix

The full sweep (`./txn bench`) runs 100 experiments:

| Parameter | Values |
|-----------|--------|
| Workload | 1, 2 |
| Protocol | occ, 2pl |
| Threads | 1, 2, 4, 8, 16 |
| Hotset probability | 0.1, 0.3, 0.5, 0.7, 0.9 |

Fixed across all runs: `--txns-per-thread 200`, `--hotset-size 10`. Each run uses a fresh database.

### Metrics Collected

Per run, per transaction type:

- **Throughput** — committed transactions per second across all threads
- **Abort rate** — aborts / (commits + aborts), expressed as a percentage
- **Average latency** — mean wall-clock time from first `Begin()` to successful `Commit()`, in microseconds. Includes all retries.
- **P50 / P90 / P99 latency** — percentiles over all committed transactions

Results are appended to `results/results.csv` (one row per transaction type per run). One representative run (workload 1, OCC, 4 threads, hotset 0.7) also dumps every individual latency sample to `results/latency_samples.csv` for distribution plots.

### CSV Schema

`results.csv`:
```
workload, protocol, threads, hotset_prob, elapsed_s,
total_commits, total_aborts, throughput_tps, abort_rate_pct,
txn_type, type_commits, type_aborts, type_abort_pct,
type_avg_latency_us, type_p50_us, type_p90_us, type_p99_us
```

`latency_samples.csv`:
```
workload, protocol, threads, hotset_prob, txn_type, latency_us
```

### Graphs

`./txn plot` generates 12 PNGs in `results/plots/`:

| File | X-axis | Y-axis | Series |
|------|--------|--------|--------|
| `w{1,2}_abort_vs_contention` | hotset_prob | abort rate % | OCC vs 2PL, per txn type |
| `w{1,2}_throughput_vs_threads` | threads | txn/s | OCC vs 2PL |
| `w{1,2}_throughput_vs_contention` | hotset_prob | txn/s | OCC vs 2PL |
| `w{1,2}_latency_vs_threads` | threads | avg latency µs | OCC vs 2PL, per txn type |
| `w{1,2}_latency_vs_contention` | hotset_prob | avg latency µs | OCC vs 2PL, per txn type |
| `w{1,2}_latency_distribution` | latency bucket | density | OCC vs 2PL, representative run |

---

## Test Coverage

### `test_occ` — 13 tests

- Read-your-writes: buffered write is visible to subsequent reads in same transaction
- Read set population: DB reads record the key for validation
- Write buffering: last write wins within a transaction, not visible until commit
- Single commit: write set flushes to DB on commit
- Read-only commit: no writes, validation still runs and succeeds
- Sequential commits: non-overlapping transactions commit without conflict
- Conflict detection: concurrent write to a key in another transaction's read set causes abort
- Disjoint key sets: no false conflicts when transactions touch different keys
- Abort semantics: clears read/write sets, leaves DB unchanged
- Timestamp monotonicity: each commit gets a strictly increasing timestamp
- Zero aborts with partitioned keys (multi-threaded)
- Balance conservation under concurrent transfers (4 threads, 200 txns each)
- High contention (3 hot keys, 4 threads) produces aborts while preserving balance invariant

### `test_2pl` — 12 tests

- `TryAcquireAll` succeeds when all keys are free
- `TryAcquireAll` fails and acquires nothing when any key is already held
- `ReleaseAll` frees keys so the next `TryAcquireAll` on the same set succeeds
- All-or-nothing: no partial lock state is left behind on failure
- Basic begin/read/write/commit flow (single-threaded)
- Read-your-writes with buffered writes
- Commit always returns `success = true`
- `retry_count = 0` when there's no contention
- Partitioned keys: zero retries, no waiting (multi-threaded)
- Balance conservation: all 800 transactions commit, invariant holds
- High contention: all transactions eventually commit
- `CommitResult.success` is always true regardless of contention (unlike OCC)
