#ifndef OCC_MANAGER_H
#define OCC_MANAGER_H

#include <atomic>
#include <vector>
#include <mutex>
#include <set>
#include <cstdint>
#include "concurrency/transaction_manager.h"
#include "database/database.h"

namespace txn {

struct CommittedTxnRecord {
    uint64_t txn_id;
    uint64_t finish_ts;
    std::set<std::string> write_keys;
};

class OCCManager : public TransactionManager {
public:
    explicit OCCManager(Database& db);

    Transaction Begin(const std::string& type_name) override;
    std::optional<std::string> Read(Transaction& txn, const std::string& key) override;
    void Write(Transaction& txn, const std::string& key, const std::string& value) override;
    CommitResult Commit(Transaction& txn) override;
    void Abort(Transaction& txn) override;
    std::string ProtocolName() const override { return "OCC"; }

private:
    bool Validate(Transaction& txn);
    void GarbageCollect(uint64_t min_active_start_ts);

    Database& db_;
    std::atomic<uint64_t> timestamp_counter_{0};
    std::atomic<uint64_t> txn_id_counter_{0};

    std::mutex validation_mutex_;
    std::mutex committed_mutex_;
    std::vector<CommittedTxnRecord> committed_history_;
};

} // namespace txn

#endif // OCC_MANAGER_H
