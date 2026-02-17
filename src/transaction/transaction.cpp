#include "transaction/transaction.h"

namespace txn {

std::optional<std::string> Transaction::Read(const std::string& key, Database& db) {
    // Read-your-writes: check write_set first
    auto it = write_set.find(key);
    if (it != write_set.end()) {
        read_set[key] = it->second;
        return it->second;
    }

    // Read from database
    auto value = db.Get(key);
    if (value.has_value()) {
        read_set[key] = value.value();
    }
    return value;
}

void Transaction::Write(const std::string& key, const std::string& value) {
    write_set[key] = value;
}

} // namespace txn
