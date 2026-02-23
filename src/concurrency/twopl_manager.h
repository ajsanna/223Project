#ifndef TWOPL_MANAGER_H
#define TWOPL_MANAGER_H

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "concurrency/transaction_manager.h"
#include "database/database.h"

namespace txn {

// Manages an exclusive-lock table for Conservative 2PL.
// All locks for a transaction are acquired atomically before execution begins.
class LockManager {
public:
    // Atomically check all keys are free, then lock them all for txn_id.
    // Returns false immediately (acquiring nothing) if any key is held.
    bool TryAcquireAll(uint64_t txn_id, const std::vector<std::string>& keys);

    // Release all locks held by txn_id for the given keys.
    void ReleaseAll(uint64_t txn_id, const std::vector<std::string>& keys);

private:
    std::unordered_map<std::string, uint64_t> lock_table_;  // 0 = free
    std::mutex table_mutex_;
};

class TwoPLManager : public TransactionManager {
public:
    explicit TwoPLManager(Database& db, int base_backoff_us = 100);

    Transaction Begin(const std::string& type_name,
                      const std::vector<std::string>& keys = {}) override;
    std::optional<std::string> Read(Transaction& txn, const std::string& key) override;
    void Write(Transaction& txn, const std::string& key, const std::string& value) override;
    CommitResult Commit(Transaction& txn) override;  // always returns success=true
    void Abort(Transaction& txn) override;
    std::string ProtocolName() const override { return "2PL"; }

private:
    Database& db_;
    LockManager lock_mgr_;
    std::atomic<uint64_t> txn_id_counter_{0};
    int base_backoff_us_;
};

}  // namespace txn

#endif  // TWOPL_MANAGER_H
