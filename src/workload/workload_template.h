#ifndef WORKLOAD_TEMPLATE_H
#define WORKLOAD_TEMPLATE_H

#include <string>
#include <vector>
#include <functional>
#include "concurrency/transaction_manager.h"

namespace txn {

struct WorkloadTemplate {
    std::string name;
    int num_input_keys;
    std::function<CommitResult(TransactionManager&, const std::vector<std::string>&)> execute;
};

inline WorkloadTemplate MakeTransferTemplate() {
    return {
        "transfer",
        2,
        [](TransactionManager& mgr, const std::vector<std::string>& keys) -> CommitResult {
            auto txn = mgr.Begin("transfer");

            auto val_a = mgr.Read(txn, keys[0]);
            auto val_b = mgr.Read(txn, keys[1]);

            int balance_a = val_a.has_value() ? std::stoi(val_a.value()) : 0;
            int balance_b = val_b.has_value() ? std::stoi(val_b.value()) : 0;

            int transfer_amount = 10;
            balance_a -= transfer_amount;
            balance_b += transfer_amount;

            mgr.Write(txn, keys[0], std::to_string(balance_a));
            mgr.Write(txn, keys[1], std::to_string(balance_b));

            return mgr.Commit(txn);
        }
    };
}

inline WorkloadTemplate MakeBalanceCheckTemplate() {
    return {
        "balance_check",
        1,
        [](TransactionManager& mgr, const std::vector<std::string>& keys) -> CommitResult {
            auto txn = mgr.Begin("balance_check");

            mgr.Read(txn, keys[0]);

            // Read-only transaction, still commits for OCC validation
            return mgr.Commit(txn);
        }
    };
}

inline WorkloadTemplate MakeWriteHeavyTemplate(int n) {
    return {
        "write_heavy",
        n,
        [n](TransactionManager& mgr, const std::vector<std::string>& keys) -> CommitResult {
            auto txn = mgr.Begin("write_heavy");

            for (int i = 0; i < n; i++) {
                auto val = mgr.Read(txn, keys[i]);
                int current = val.has_value() ? std::stoi(val.value()) : 0;
                mgr.Write(txn, keys[i], std::to_string(current + 1));
            }

            return mgr.Commit(txn);
        }
    };
}

} // namespace txn

#endif // WORKLOAD_TEMPLATE_H
