#include "database/database.h"
#include "transaction/transaction.h"
#include "concurrency/twopl_manager.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <filesystem>

using namespace txn;

// Helper: open a fresh database for each test
static Database& fresh_db(const std::string& path = "test_2pl_db") {
    static Database db;
    if (db.IsOpen()) db.Close();
    std::filesystem::remove_all(path);
    assert(db.Open(path));
    return db;
}

// ============================================================
// Phase 1: LockManager unit tests
// ============================================================

void test_lock_acquire_all_free() {
    std::cout << "\n=== Test: TryAcquireAll succeeds when all keys free ===" << std::endl;

    LockManager lm;
    std::vector<std::string> keys = {"a", "b", "c"};

    bool ok = lm.TryAcquireAll(1, keys);
    assert(ok);
    std::cout << "  PASSED: Acquired 3 free keys" << std::endl;

    lm.ReleaseAll(1, keys);
}

void test_lock_acquire_fails_if_held() {
    std::cout << "\n=== Test: TryAcquireAll fails when any key is held ===" << std::endl;

    LockManager lm;
    std::vector<std::string> keys_txn1 = {"a", "b"};
    std::vector<std::string> keys_txn2 = {"b", "c"};

    // txn 1 acquires "a" and "b"
    bool ok1 = lm.TryAcquireAll(1, keys_txn1);
    assert(ok1);

    // txn 2 tries to acquire "b" and "c" — "b" is held, so must fail
    bool ok2 = lm.TryAcquireAll(2, keys_txn2);
    assert(!ok2);
    std::cout << "  PASSED: TryAcquireAll correctly rejects when a key is held" << std::endl;

    lm.ReleaseAll(1, keys_txn1);
}

void test_lock_release_allows_reacquire() {
    std::cout << "\n=== Test: ReleaseAll frees keys for next acquire ===" << std::endl;

    LockManager lm;
    std::vector<std::string> keys = {"x", "y"};

    assert(lm.TryAcquireAll(10, keys));
    lm.ReleaseAll(10, keys);

    // After release, another txn can acquire the same keys
    bool ok = lm.TryAcquireAll(11, keys);
    assert(ok);
    std::cout << "  PASSED: Keys re-acquirable after release" << std::endl;

    lm.ReleaseAll(11, keys);
}

void test_lock_all_or_nothing_no_partial_hold() {
    std::cout << "\n=== Test: All-or-nothing — no partial hold left on failure ===" << std::endl;

    LockManager lm;

    // txn 1 holds "b"
    assert(lm.TryAcquireAll(1, {"b"}));

    // txn 2 wants "a" and "b"; fails because "b" is held
    bool ok2 = lm.TryAcquireAll(2, {"a", "b"});
    assert(!ok2);

    // Release txn 1's hold, then txn 3 acquires "a" alone — should succeed
    // (proving "a" was never partially locked by txn 2)
    lm.ReleaseAll(1, {"b"});
    bool ok3 = lm.TryAcquireAll(3, {"a"});
    assert(ok3);
    std::cout << "  PASSED: No partial lock state left after failed TryAcquireAll" << std::endl;

    lm.ReleaseAll(3, {"a"});
}

// ============================================================
// Phase 2: TwoPLManager single-threaded tests
// ============================================================

void test_2pl_basic_commit() {
    std::cout << "\n=== Test: Basic Begin/Read/Write/Commit ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "100");

    TwoPLManager mgr(db);

    auto txn = mgr.Begin("test", {"k1"});
    assert(txn.status == TxnStatus::ACTIVE);

    auto val = mgr.Read(txn, "k1");
    assert(val.has_value());
    assert(val.value() == "100");

    mgr.Write(txn, "k1", "200");
    auto result = mgr.Commit(txn);

    assert(result.success);
    assert(txn.status == TxnStatus::COMMITTED);
    assert(db.Get("k1").value() == "200");
    std::cout << "  PASSED: Basic 2PL commit writes to DB" << std::endl;

    db.Close();
}

void test_2pl_read_your_writes() {
    std::cout << "\n=== Test: Read-Your-Writes ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "original");

    TwoPLManager mgr(db);

    auto txn = mgr.Begin("ryw", {"k1"});
    mgr.Write(txn, "k1", "buffered");

    auto val = mgr.Read(txn, "k1");
    assert(val.has_value());
    assert(val.value() == "buffered");  // sees own write
    assert(db.Get("k1").value() == "original");  // DB unchanged until commit

    mgr.Commit(txn);
    assert(db.Get("k1").value() == "buffered");
    std::cout << "  PASSED: Read returns buffered value before commit" << std::endl;

    db.Close();
}

void test_2pl_commit_always_success() {
    std::cout << "\n=== Test: Commit always returns success=true ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "10");
    db.Put("k2", "20");

    TwoPLManager mgr(db);

    // Run several sequential transactions — each must succeed
    for (int i = 0; i < 5; i++) {
        auto txn = mgr.Begin("seq", {"k1", "k2"});
        auto v1 = mgr.Read(txn, "k1");
        mgr.Write(txn, "k1", std::to_string(std::stoi(v1.value_or("0")) + 1));
        auto result = mgr.Commit(txn);
        assert(result.success);
    }
    std::cout << "  PASSED: All 5 sequential 2PL commits return success=true" << std::endl;

    db.Close();
}

void test_2pl_no_contention_zero_retries() {
    std::cout << "\n=== Test: retry_count == 0 when no contention ===" << std::endl;

    auto& db = fresh_db();
    TwoPLManager mgr(db);

    auto txn = mgr.Begin("no_wait", {"unique_key_42"});
    assert(txn.retry_count == 0);

    mgr.Commit(txn);
    std::cout << "  PASSED: retry_count is 0 with no lock contention" << std::endl;

    db.Close();
}

// ============================================================
// Phase 3: Multi-threaded correctness
// ============================================================

void test_2pl_partitioned_zero_retries() {
    std::cout << "\n=== Test: Partitioned Keys — Zero Lock Retries ===" << std::endl;

    auto& db = fresh_db();
    const int NUM_KEYS = 400;
    const int NUM_THREADS = 4;
    const int TXNS_PER_THREAD = 50;

    for (int i = 0; i < NUM_KEYS; i++) {
        db.Put("key_" + std::to_string(i), "0");
    }

    TwoPLManager mgr(db);
    std::atomic<int> total_retries{0};

    auto worker = [&](int thread_id) {
        int partition_size = NUM_KEYS / NUM_THREADS;
        int start = thread_id * partition_size;

        for (int i = 0; i < TXNS_PER_THREAD; i++) {
            // Each thread accesses its own disjoint key partition
            int idx = start + (i % partition_size);
            std::string key = "key_" + std::to_string(idx);

            auto txn = mgr.Begin("partitioned", {key});
            total_retries += txn.retry_count;

            auto val = mgr.Read(txn, key);
            int cur = std::stoi(val.value_or("0"));
            mgr.Write(txn, key, std::to_string(cur + 1));
            auto result = mgr.Commit(txn);
            assert(result.success);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }
    for (auto& t : threads) t.join();

    std::cout << "  Lock retries with partitioned keys: " << total_retries.load() << std::endl;
    assert(total_retries.load() == 0);
    std::cout << "  PASSED: Zero lock retries with disjoint key partitions" << std::endl;

    db.Close();
}

void test_2pl_balance_conservation() {
    std::cout << "\n=== Test: Balance Conservation Under Concurrent Transfers ===" << std::endl;

    auto& db = fresh_db();
    const int NUM_ACCOUNTS = 100;
    const int INITIAL_BALANCE = 1000;
    const int NUM_THREADS = 4;
    const int TXNS_PER_THREAD = 200;
    const long long EXPECTED_TOTAL = (long long)NUM_ACCOUNTS * INITIAL_BALANCE;

    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        db.Put("account_" + std::to_string(i), std::to_string(INITIAL_BALANCE));
    }

    TwoPLManager mgr(db);
    std::atomic<int> total_commits{0};

    auto worker = [&](int thread_id) {
        std::mt19937 rng(thread_id * 1000 + 99);
        std::uniform_int_distribution<int> acct_dist(0, NUM_ACCOUNTS - 1);

        for (int i = 0; i < TXNS_PER_THREAD; i++) {
            int a = acct_dist(rng);
            int b;
            do { b = acct_dist(rng); } while (b == a);

            std::string key_a = "account_" + std::to_string(a);
            std::string key_b = "account_" + std::to_string(b);

            // Conservative 2PL: pass both keys to Begin — lock before execution
            auto txn = mgr.Begin("transfer", {key_a, key_b});

            auto val_a = mgr.Read(txn, key_a);
            auto val_b = mgr.Read(txn, key_b);

            int bal_a = std::stoi(val_a.value_or("0"));
            int bal_b = std::stoi(val_b.value_or("0"));

            mgr.Write(txn, key_a, std::to_string(bal_a - 10));
            mgr.Write(txn, key_b, std::to_string(bal_b + 10));

            auto result = mgr.Commit(txn);
            assert(result.success);  // 2PL never fails commit
            total_commits++;
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }
    for (auto& t : threads) t.join();

    long long total_balance = 0;
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        auto val = db.Get("account_" + std::to_string(i));
        assert(val.has_value());
        total_balance += std::stoi(val.value());
    }

    std::cout << "  Commits: " << total_commits.load() << " (no aborts with 2PL)" << std::endl;
    std::cout << "  Expected total: " << EXPECTED_TOTAL
              << ", Actual: " << total_balance << std::endl;

    assert(total_balance == EXPECTED_TOTAL);
    assert(total_commits.load() == NUM_THREADS * TXNS_PER_THREAD);
    std::cout << "  PASSED: Balance conserved and all transactions committed" << std::endl;

    db.Close();
}

void test_2pl_high_contention_all_commit() {
    std::cout << "\n=== Test: High Contention — All Transactions Eventually Commit ===" << std::endl;

    auto& db = fresh_db();
    const int NUM_THREADS = 4;
    const int TXNS_PER_THREAD = 100;

    // Only 3 hot keys — extreme contention
    db.Put("hot_0", "0");
    db.Put("hot_1", "0");
    db.Put("hot_2", "0");

    TwoPLManager mgr(db);
    std::atomic<int> total_commits{0};
    std::atomic<int> total_retries{0};

    auto worker = [&](int thread_id) {
        std::mt19937 rng(thread_id * 13 + 7);
        std::uniform_int_distribution<int> key_dist(0, 2);

        for (int i = 0; i < TXNS_PER_THREAD; i++) {
            int k1 = key_dist(rng);
            int k2;
            do { k2 = key_dist(rng); } while (k2 == k1);

            std::string key_a = "hot_" + std::to_string(k1);
            std::string key_b = "hot_" + std::to_string(k2);

            auto txn = mgr.Begin("hot_transfer", {key_a, key_b});
            total_retries += txn.retry_count;

            auto va = mgr.Read(txn, key_a);
            auto vb = mgr.Read(txn, key_b);

            int a = std::stoi(va.value_or("0"));
            int b = std::stoi(vb.value_or("0"));

            mgr.Write(txn, key_a, std::to_string(a - 1));
            mgr.Write(txn, key_b, std::to_string(b + 1));

            auto result = mgr.Commit(txn);
            assert(result.success);  // 2PL always succeeds
            total_commits++;
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }
    for (auto& t : threads) t.join();

    std::cout << "  Commits: " << total_commits.load()
              << ", Lock retries (backoffs): " << total_retries.load() << std::endl;

    assert(total_commits.load() == NUM_THREADS * TXNS_PER_THREAD);
    assert(total_retries.load() > 0);  // contention caused some retries

    // Balance must be conserved
    long long total = 0;
    for (int i = 0; i < 3; i++) {
        total += std::stoi(db.Get("hot_" + std::to_string(i)).value());
    }
    assert(total == 0);
    std::cout << "  PASSED: All transactions committed, balance conserved under high contention" << std::endl;

    db.Close();
}

void test_2pl_commit_result_always_true() {
    std::cout << "\n=== Test: CommitResult.success is always true (unlike OCC) ===" << std::endl;

    auto& db = fresh_db();
    db.Put("shared", "0");

    TwoPLManager mgr(db);
    std::atomic<int> false_commits{0};
    const int NUM_THREADS = 4;
    const int TXNS = 50;

    auto worker = [&]() {
        for (int i = 0; i < TXNS; i++) {
            auto txn = mgr.Begin("inc", {"shared"});
            auto val = mgr.Read(txn, "shared");
            int cur = std::stoi(val.value_or("0"));
            mgr.Write(txn, "shared", std::to_string(cur + 1));
            auto result = mgr.Commit(txn);
            if (!result.success) false_commits++;
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    assert(false_commits.load() == 0);
    std::cout << "  PASSED: CommitResult.success always true across "
              << NUM_THREADS * TXNS << " transactions" << std::endl;

    db.Close();
}

// ============================================================
// Main
// ============================================================

int main() {
    std::cout << "Starting 2PL Tests" << std::endl;
    std::cout << "==================" << std::endl;

    try {
        // Phase 1: LockManager unit tests
        test_lock_acquire_all_free();
        test_lock_acquire_fails_if_held();
        test_lock_release_allows_reacquire();
        test_lock_all_or_nothing_no_partial_hold();

        // Phase 2: TwoPLManager single-threaded
        test_2pl_basic_commit();
        test_2pl_read_your_writes();
        test_2pl_commit_always_success();
        test_2pl_no_contention_zero_retries();

        // Phase 3: Multi-threaded correctness
        test_2pl_partitioned_zero_retries();
        test_2pl_balance_conservation();
        test_2pl_high_contention_all_commit();
        test_2pl_commit_result_always_true();

        std::cout << "\n==================" << std::endl;
        std::cout << "All 2PL Tests Passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nTEST FAILED with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
