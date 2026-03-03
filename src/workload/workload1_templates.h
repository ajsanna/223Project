#ifndef WORKLOAD1_TEMPLATES_H
#define WORKLOAD1_TEMPLATES_H

#include "workload/workload_template.h"
#include "workload/record.h"

namespace txn {

// Transfer template for workload 1.
// Keys: [A_src, A_dst] — decrements src balance by 1, increments dst balance by 1.
// key_builder must be injected in main.cpp with account_keys.
inline WorkloadTemplate MakeW1TransferTemplate() {
    return {
        "transfer",
        2,
        nullptr, // key_builder injected in main.cpp
        [](TransactionManager& mgr, const std::vector<std::string>& keys) -> CommitResult {
            auto txn = mgr.Begin("transfer", keys);

            auto val_a = mgr.Read(txn, keys[0]);
            auto val_b = mgr.Read(txn, keys[1]);

            Record rec_a = val_a.has_value() ? DeserializeRecord(val_a.value()) : Record{};
            Record rec_b = val_b.has_value() ? DeserializeRecord(val_b.value()) : Record{};

            SetIntField(rec_a, "balance", GetIntField(rec_a, "balance") - 1);
            SetIntField(rec_b, "balance", GetIntField(rec_b, "balance") + 1);

            mgr.Write(txn, keys[0], SerializeRecord(rec_a));
            mgr.Write(txn, keys[1], SerializeRecord(rec_b));

            return mgr.Commit(txn);
        }
    };
}

} // namespace txn

#endif // WORKLOAD1_TEMPLATES_H
