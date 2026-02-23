#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <cstdint>
#include <vector>
#include "database/database.h"

namespace txn {

enum class TxnStatus {
    ACTIVE,
    COMMITTED,
    ABORTED
};

struct Transaction {
    uint64_t txn_id;
    std::string type_name;
    uint64_t start_ts;
    uint64_t validation_ts = 0;
    uint64_t finish_ts = 0;
    TxnStatus status = TxnStatus::ACTIVE;

    std::unordered_map<std::string, std::string> read_set;
    std::unordered_map<std::string, std::string> write_set;

    std::vector<std::string> lock_keys;  // keys held under 2PL (empty for OCC)

    std::chrono::steady_clock::time_point wall_start;
    int retry_count = 0;

    // Read: check write_set first (read-your-writes), else read from DB
    std::optional<std::string> Read(const std::string& key, Database& db);

    // Write: buffer in write_set only
    void Write(const std::string& key, const std::string& value);
};

} // namespace txn

#endif // TRANSACTION_H
