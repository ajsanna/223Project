#include "concurrency/occ_manager.h"
#include <algorithm>
#include <vector>

namespace txn {

OCCManager::OCCManager(Database& db) : db_(db) {}

Transaction OCCManager::Begin(const std::string& type_name,
                              const std::vector<std::string>& /*keys*/) {
    Transaction txn;
    txn.txn_id = ++txn_id_counter_;
    txn.type_name = type_name;
    txn.start_ts = timestamp_counter_.load();
    txn.status = TxnStatus::ACTIVE;
    txn.wall_start = std::chrono::steady_clock::now();
    return txn;
}

std::optional<std::string> OCCManager::Read(Transaction& txn, const std::string& key) {
    return txn.Read(key, db_);
}

void OCCManager::Write(Transaction& txn, const std::string& key, const std::string& value) {
    txn.Write(key, value);
}

bool OCCManager::Validate(Transaction& txn) {
    // Check for conflicts with committed transactions
    std::lock_guard<std::mutex> lock(committed_mutex_);
    for (const auto& record : committed_history_) {
        if (record.finish_ts > txn.start_ts) {
            // Check if any of the committed txn's write keys overlap with our read set
            for (const auto& write_key : record.write_keys) {
                if (txn.read_set.find(write_key) != txn.read_set.end()) {
                    return false;
                }
            }
        }
    }
    return true;
}

CommitResult OCCManager::Commit(Transaction& txn) {
    std::lock_guard<std::mutex> val_lock(validation_mutex_);

    // Assign validation timestamp
    txn.validation_ts = ++timestamp_counter_;

    // Validate
    if (!Validate(txn)) {
        txn.status = TxnStatus::ABORTED;
        return {false, txn.txn_id, txn.retry_count};
    }

    // Apply writes to database
    for (const auto& [key, value] : txn.write_set) {
        db_.Put(key, value);
    }

    // Assign finish timestamp
    txn.finish_ts = ++timestamp_counter_;
    txn.status = TxnStatus::COMMITTED;

    // Record committed transaction
    CommittedTxnRecord record;
    record.txn_id = txn.txn_id;
    record.finish_ts = txn.finish_ts;
    for (const auto& [key, _] : txn.write_set) {
        record.write_keys.insert(key);
    }

    {
        std::lock_guard<std::mutex> lock(committed_mutex_);
        committed_history_.push_back(std::move(record));
    }

    return {true, txn.txn_id, txn.retry_count};
}

void OCCManager::Abort(Transaction& txn) {
    txn.status = TxnStatus::ABORTED;
    txn.read_set.clear();
    txn.write_set.clear();
}

void OCCManager::GarbageCollect(uint64_t min_active_start_ts) {
    std::lock_guard<std::mutex> lock(committed_mutex_);
    committed_history_.erase(
        std::remove_if(committed_history_.begin(), committed_history_.end(),
            [min_active_start_ts](const CommittedTxnRecord& r) {
                return r.finish_ts <= min_active_start_ts;
            }),
        committed_history_.end()
    );
}

} // namespace txn
