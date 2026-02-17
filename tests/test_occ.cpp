#include "database/database.h"
#include "transaction/transaction.h"
#include "concurrency/occ_manager.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <cmath>
#include <filesystem>

using namespace txn;

// Helper: open a fresh database for each test
static Database& fresh_db(const std::string& path = "test_occ_db") {
    static Database db;
    if (db.IsOpen()) db.Close();
    // Remove old data
    std::filesystem::remove_all(path);
    assert(db.Open(path));
    return db;
}

// ============================================================
// Phase 1: Transaction struct tests
// ============================================================

void test_transaction_read_your_writes() {
    std::cout << "\n=== Test: Read-Your-Writes ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "original");

    Transaction txn;
    txn.txn_id = 1;

    // Write to the buffer
    txn.Write("k1", "buffered");

    // Read should return the buffered value, not the DB value
    auto val = txn.Read("k1", db);
    assert(val.has_value());
    assert(val.value() == "buffered");
    std::cout << "  PASSED: Read returns buffered write" << std::endl;

    // DB should still have the original value
    auto db_val = db.Get("k1");
    assert(db_val.has_value());
    assert(db_val.value() == "original");
    std::cout << "  PASSED: DB unchanged until commit" << std::endl;

    db.Close();
}

void test_transaction_read_from_db() {
    std::cout << "\n=== Test: Read From DB ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "from_db");

    Transaction txn;
    txn.txn_id = 1;

    auto val = txn.Read("k1", db);
    assert(val.has_value());
    assert(val.value() == "from_db");

    // Should be recorded in read_set
    assert(txn.read_set.count("k1") == 1);
    assert(txn.read_set["k1"] == "from_db");
    std::cout << "  PASSED: Read populates read_set from DB" << std::endl;

    // Read a non-existent key
    auto val2 = txn.Read("missing", db);
    assert(!val2.has_value());
    std::cout << "  PASSED: Read of missing key returns nullopt" << std::endl;

    db.Close();
}

void test_transaction_write_buffering() {
    std::cout << "\n=== Test: Write Buffering ===" << std::endl;

    Transaction txn;
    txn.txn_id = 1;

    txn.Write("a", "1");
    txn.Write("b", "2");
    txn.Write("a", "3"); // overwrite

    assert(txn.write_set.size() == 2);
    assert(txn.write_set["a"] == "3");
    assert(txn.write_set["b"] == "2");
    std::cout << "  PASSED: Writes buffered correctly, last-write wins" << std::endl;
}

// ============================================================
// Phase 2: OCC Manager tests
// ============================================================

void test_occ_single_txn_commit() {
    std::cout << "\n=== Test: Single Transaction Commit ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "100");

    OCCManager mgr(db);

    auto txn = mgr.Begin("test");
    auto val = mgr.Read(txn, "k1");
    assert(val.has_value());
    assert(val.value() == "100");

    mgr.Write(txn, "k1", "200");
    auto result = mgr.Commit(txn);

    assert(result.success);
    assert(txn.status == TxnStatus::COMMITTED);

    // Verify DB was updated
    auto db_val = db.Get("k1");
    assert(db_val.has_value());
    assert(db_val.value() == "200");
    std::cout << "  PASSED: Single txn commits and writes to DB" << std::endl;

    db.Close();
}

void test_occ_read_only_commit() {
    std::cout << "\n=== Test: Read-Only Transaction Commit ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "500");

    OCCManager mgr(db);

    auto txn = mgr.Begin("read_only");
    auto val = mgr.Read(txn, "k1");
    assert(val.has_value());
    assert(val.value() == "500");

    auto result = mgr.Commit(txn);
    assert(result.success);

    // DB should be unchanged
    assert(db.Get("k1").value() == "500");
    std::cout << "  PASSED: Read-only txn commits without modifying DB" << std::endl;

    db.Close();
}

void test_occ_sequential_no_conflict() {
    std::cout << "\n=== Test: Sequential Transactions No Conflict ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "100");
    db.Put("k2", "200");

    OCCManager mgr(db);

    // Txn 1: read k1, write k1
    auto txn1 = mgr.Begin("t1");
    mgr.Read(txn1, "k1");
    mgr.Write(txn1, "k1", "150");
    auto r1 = mgr.Commit(txn1);
    assert(r1.success);

    // Txn 2: read k1 (sees 150), write k2
    auto txn2 = mgr.Begin("t2");
    auto val = mgr.Read(txn2, "k1");
    assert(val.value() == "150");
    mgr.Write(txn2, "k2", "250");
    auto r2 = mgr.Commit(txn2);
    assert(r2.success);

    assert(db.Get("k1").value() == "150");
    assert(db.Get("k2").value() == "250");
    std::cout << "  PASSED: Sequential txns commit without conflict" << std::endl;

    db.Close();
}

void test_occ_conflict_detection() {
    std::cout << "\n=== Test: OCC Conflict Detection ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "100");

    OCCManager mgr(db);

    // Txn A starts, reads k1
    auto txnA = mgr.Begin("A");
    mgr.Read(txnA, "k1");

    // Txn B starts, reads and writes k1, commits first
    auto txnB = mgr.Begin("B");
    mgr.Read(txnB, "k1");
    mgr.Write(txnB, "k1", "200");
    auto rB = mgr.Commit(txnB);
    assert(rB.success);
    std::cout << "  Txn B committed (wrote k1=200)" << std::endl;

    // Now Txn A tries to commit — should fail because B wrote k1
    // after A started, and A read k1
    mgr.Write(txnA, "k1", "300");
    auto rA = mgr.Commit(txnA);
    assert(!rA.success);
    assert(txnA.status == TxnStatus::ABORTED);
    std::cout << "  PASSED: Txn A correctly aborted due to write-read conflict" << std::endl;

    // DB should have B's value, not A's
    assert(db.Get("k1").value() == "200");
    std::cout << "  PASSED: DB reflects committed txn only" << std::endl;

    db.Close();
}

void test_occ_no_conflict_disjoint_keys() {
    std::cout << "\n=== Test: No Conflict on Disjoint Keys ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "100");
    db.Put("k2", "200");

    OCCManager mgr(db);

    // Txn A reads k1
    auto txnA = mgr.Begin("A");
    mgr.Read(txnA, "k1");

    // Txn B writes k2 and commits
    auto txnB = mgr.Begin("B");
    mgr.Read(txnB, "k2");
    mgr.Write(txnB, "k2", "250");
    auto rB = mgr.Commit(txnB);
    assert(rB.success);

    // Txn A writes k1 — no conflict since B only touched k2
    mgr.Write(txnA, "k1", "150");
    auto rA = mgr.Commit(txnA);
    assert(rA.success);
    std::cout << "  PASSED: Disjoint key sets don't conflict" << std::endl;

    db.Close();
}

void test_occ_abort_clears_state() {
    std::cout << "\n=== Test: Abort Clears Read/Write Sets ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "100");

    OCCManager mgr(db);

    auto txn = mgr.Begin("test");
    mgr.Read(txn, "k1");
    mgr.Write(txn, "k1", "999");

    mgr.Abort(txn);

    assert(txn.status == TxnStatus::ABORTED);
    assert(txn.read_set.empty());
    assert(txn.write_set.empty());
    assert(db.Get("k1").value() == "100"); // unchanged
    std::cout << "  PASSED: Abort clears sets, DB unchanged" << std::endl;

    db.Close();
}

void test_occ_timestamp_monotonicity() {
    std::cout << "\n=== Test: Timestamp Monotonicity ===" << std::endl;

    auto& db = fresh_db();
    db.Put("k1", "0");

    OCCManager mgr(db);

    uint64_t prev_finish = 0;
    for (int i = 0; i < 10; i++) {
        auto txn = mgr.Begin("seq");
        mgr.Read(txn, "k1");
        mgr.Write(txn, "k1", std::to_string(i));
        auto r = mgr.Commit(txn);
        assert(r.success);
        assert(txn.validation_ts > 0);
        assert(txn.finish_ts > txn.validation_ts);
        assert(txn.finish_ts > prev_finish);
        prev_finish = txn.finish_ts;
    }
    std::cout << "  PASSED: Timestamps strictly increase across commits" << std::endl;

    db.Close();
}

// ============================================================
// Phase 3: Multi-threaded correctness
// ============================================================

void test_occ_multithread_balance_conservation() {
    std::cout << "\n=== Test: Multi-Threaded Balance Conservation ===" << std::endl;

    auto& db = fresh_db();
    const int NUM_ACCOUNTS = 100;
    const int INITIAL_BALANCE = 1000;
    const int NUM_THREADS = 4;
    const int TXNS_PER_THREAD = 200;
    const long long EXPECTED_TOTAL = (long long)NUM_ACCOUNTS * INITIAL_BALANCE;

    // Initialize accounts
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        db.Put("account_" + std::to_string(i), std::to_string(INITIAL_BALANCE));
    }

    OCCManager mgr(db);
    std::atomic<int> total_commits{0};
    std::atomic<int> total_aborts{0};

    auto worker = [&](int thread_id) {
        std::mt19937 rng(thread_id * 1000 + 42);
        std::uniform_int_distribution<int> acct_dist(0, NUM_ACCOUNTS - 1);

        for (int i = 0; i < TXNS_PER_THREAD; i++) {
            // Pick two distinct accounts
            int a = acct_dist(rng);
            int b;
            do { b = acct_dist(rng); } while (b == a);

            std::string key_a = "account_" + std::to_string(a);
            std::string key_b = "account_" + std::to_string(b);

            while (true) {
                auto txn = mgr.Begin("transfer");
                auto val_a = mgr.Read(txn, key_a);
                auto val_b = mgr.Read(txn, key_b);

                int bal_a = std::stoi(val_a.value_or("0"));
                int bal_b = std::stoi(val_b.value_or("0"));

                mgr.Write(txn, key_a, std::to_string(bal_a - 10));
                mgr.Write(txn, key_b, std::to_string(bal_b + 10));

                auto result = mgr.Commit(txn);
                if (result.success) {
                    total_commits++;
                    break;
                } else {
                    total_aborts++;
                    // Small backoff
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }
    for (auto& t : threads) {
        t.join();
    }

    // Verify balance conservation
    long long total_balance = 0;
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        auto val = db.Get("account_" + std::to_string(i));
        assert(val.has_value());
        total_balance += std::stoi(val.value());
    }

    std::cout << "  Commits: " << total_commits.load()
              << ", Aborts: " << total_aborts.load() << std::endl;
    std::cout << "  Expected total: " << EXPECTED_TOTAL
              << ", Actual: " << total_balance << std::endl;

    assert(total_balance == EXPECTED_TOTAL);
    assert(total_commits.load() == NUM_THREADS * TXNS_PER_THREAD);
    std::cout << "  PASSED: Balance conserved under concurrent transfers" << std::endl;

    db.Close();
}

void test_occ_multithread_all_commit_low_contention() {
    std::cout << "\n=== Test: Low Contention All Commit ===" << std::endl;

    auto& db = fresh_db();
    const int NUM_KEYS = 1000;
    const int NUM_THREADS = 4;
    const int TXNS_PER_THREAD = 50;

    for (int i = 0; i < NUM_KEYS; i++) {
        db.Put("key_" + std::to_string(i), "0");
    }

    OCCManager mgr(db);
    std::atomic<int> total_aborts{0};

    // Each thread writes to its own partition of keys — no overlap
    auto worker = [&](int thread_id) {
        int start_key = thread_id * (NUM_KEYS / NUM_THREADS);

        for (int i = 0; i < TXNS_PER_THREAD; i++) {
            int key_idx = start_key + (i % (NUM_KEYS / NUM_THREADS));
            std::string key = "key_" + std::to_string(key_idx);

            while (true) {
                auto txn = mgr.Begin("partitioned_write");
                auto val = mgr.Read(txn, key);
                int cur = std::stoi(val.value_or("0"));
                mgr.Write(txn, key, std::to_string(cur + 1));

                auto result = mgr.Commit(txn);
                if (result.success) break;
                total_aborts++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "  Aborts with partitioned keys: " << total_aborts.load() << std::endl;
    assert(total_aborts.load() == 0);
    std::cout << "  PASSED: Zero aborts when threads access disjoint keys" << std::endl;

    db.Close();
}

void test_occ_contention_increases_aborts() {
    std::cout << "\n=== Test: Higher Contention -> More Aborts ===" << std::endl;

    auto& db = fresh_db();
    const int NUM_THREADS = 4;
    const int TXNS_PER_THREAD = 100;

    // Only 3 keys — very high contention
    db.Put("hot_0", "0");
    db.Put("hot_1", "0");
    db.Put("hot_2", "0");

    OCCManager mgr(db);
    std::atomic<int> total_aborts{0};

    auto worker = [&](int thread_id) {
        std::mt19937 rng(thread_id * 7 + 1);
        std::uniform_int_distribution<int> key_dist(0, 2);

        for (int i = 0; i < TXNS_PER_THREAD; i++) {
            int k1 = key_dist(rng);
            int k2;
            do { k2 = key_dist(rng); } while (k2 == k1);

            std::string key_a = "hot_" + std::to_string(k1);
            std::string key_b = "hot_" + std::to_string(k2);

            while (true) {
                auto txn = mgr.Begin("hot_transfer");
                auto va = mgr.Read(txn, key_a);
                auto vb = mgr.Read(txn, key_b);

                int a = std::stoi(va.value_or("0"));
                int b = std::stoi(vb.value_or("0"));

                mgr.Write(txn, key_a, std::to_string(a - 1));
                mgr.Write(txn, key_b, std::to_string(b + 1));

                auto result = mgr.Commit(txn);
                if (result.success) break;
                total_aborts++;
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "  Aborts with 3 hot keys, 4 threads: " << total_aborts.load() << std::endl;
    assert(total_aborts.load() > 0);
    std::cout << "  PASSED: High contention produces aborts as expected" << std::endl;

    // Verify conservation still holds
    long long total = 0;
    for (int i = 0; i < 3; i++) {
        total += std::stoi(db.Get("hot_" + std::to_string(i)).value());
    }
    assert(total == 0);
    std::cout << "  PASSED: Balance still conserved under high contention" << std::endl;

    db.Close();
}

// ============================================================
// Main
// ============================================================

int main() {
    std::cout << "Starting OCC Tests" << std::endl;
    std::cout << "==================" << std::endl;

    try {
        // Transaction struct tests
        test_transaction_read_your_writes();
        test_transaction_read_from_db();
        test_transaction_write_buffering();

        // OCC single-threaded tests
        test_occ_single_txn_commit();
        test_occ_read_only_commit();
        test_occ_sequential_no_conflict();
        test_occ_conflict_detection();
        test_occ_no_conflict_disjoint_keys();
        test_occ_abort_clears_state();
        test_occ_timestamp_monotonicity();

        // Multi-threaded tests
        test_occ_multithread_all_commit_low_contention();
        test_occ_multithread_balance_conservation();
        test_occ_contention_increases_aborts();

        std::cout << "\n==================" << std::endl;
        std::cout << "All OCC Tests Passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nTEST FAILED with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
