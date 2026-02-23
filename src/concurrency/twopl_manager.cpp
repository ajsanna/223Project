#include "concurrency/twopl_manager.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>

namespace txn {

// ---------------------------------------------------------------------------
// LockManager
// ---------------------------------------------------------------------------

bool LockManager::TryAcquireAll(uint64_t txn_id,
                                 const std::vector<std::string>& keys) {
    std::lock_guard<std::mutex> guard(table_mutex_);

    // Phase 1: check all keys are free (all-or-nothing)
    for (const auto& key : keys) {
        auto it = lock_table_.find(key);
        if (it != lock_table_.end() && it->second != 0) {
            return false;
        }
    }

    // Phase 2: acquire all
    for (const auto& key : keys) {
        lock_table_[key] = txn_id;
    }
    return true;
}

void LockManager::ReleaseAll(uint64_t txn_id,
                              const std::vector<std::string>& keys) {
    std::lock_guard<std::mutex> guard(table_mutex_);
    for (const auto& key : keys) {
        auto it = lock_table_.find(key);
        if (it != lock_table_.end() && it->second == txn_id) {
            lock_table_.erase(it);
        }
    }
}

// ---------------------------------------------------------------------------
// TwoPLManager
// ---------------------------------------------------------------------------

TwoPLManager::TwoPLManager(Database& db, int base_backoff_us)
    : db_(db), base_backoff_us_(base_backoff_us) {}

Transaction TwoPLManager::Begin(const std::string& type_name,
                                 const std::vector<std::string>& keys) {
    Transaction txn;
    txn.txn_id = ++txn_id_counter_;
    txn.type_name = type_name;
    txn.start_ts = 0;  // 2PL does not use timestamps
    txn.lock_keys = keys;
    txn.status = TxnStatus::ACTIVE;
    txn.wall_start = std::chrono::steady_clock::now();

    // Conservative 2PL: acquire ALL locks before any execution.
    // Use exponential backoff + jitter to prevent livelock.
    thread_local std::mt19937 rng(std::random_device{}());
    int retry = 0;
    while (!lock_mgr_.TryAcquireAll(txn.txn_id, keys)) {
        int cap = std::min(retry, 10);
        int backoff_us = base_backoff_us_ * (1 << cap);
        std::uniform_int_distribution<int> jitter(0, backoff_us / 2);
        std::this_thread::sleep_for(
            std::chrono::microseconds(backoff_us + jitter(rng)));
        retry++;
    }
    txn.retry_count = retry;
    return txn;
}

std::optional<std::string> TwoPLManager::Read(Transaction& txn,
                                               const std::string& key) {
    return txn.Read(key, db_);
}

void TwoPLManager::Write(Transaction& txn, const std::string& key,
                          const std::string& value) {
    txn.Write(key, value);
}

CommitResult TwoPLManager::Commit(Transaction& txn) {
    // Apply buffered writes to the database
    for (const auto& [key, value] : txn.write_set) {
        db_.Put(key, value);
    }

    txn.status = TxnStatus::COMMITTED;

    // Release all locks — 2PL shrinking phase
    lock_mgr_.ReleaseAll(txn.txn_id, txn.lock_keys);

    // 2PL commit always succeeds; no validation step needed
    return {true, txn.txn_id, txn.retry_count};
}

void TwoPLManager::Abort(Transaction& txn) {
    txn.status = TxnStatus::ABORTED;
    txn.read_set.clear();
    txn.write_set.clear();

    // Release all locks
    lock_mgr_.ReleaseAll(txn.txn_id, txn.lock_keys);
}

}  // namespace txn
