#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

#include <string>
#include <vector>
#include <optional>
#include "transaction/transaction.h"

namespace txn {

struct CommitResult {
    bool success;
    uint64_t txn_id;
    int retries;
};

class TransactionManager {
public:
    virtual ~TransactionManager() = default;

    virtual Transaction Begin(const std::string& type_name,
                              const std::vector<std::string>& keys = {}) = 0;
    virtual std::optional<std::string> Read(Transaction& txn, const std::string& key) = 0;
    virtual void Write(Transaction& txn, const std::string& key, const std::string& value) = 0;
    virtual CommitResult Commit(Transaction& txn) = 0;
    virtual void Abort(Transaction& txn) = 0;
    virtual std::string ProtocolName() const = 0;
};

} // namespace txn

#endif // TRANSACTION_MANAGER_H
