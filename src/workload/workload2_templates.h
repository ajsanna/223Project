#ifndef WORKLOAD2_TEMPLATES_H
#define WORKLOAD2_TEMPLATES_H

#include "workload/workload_template.h"
#include "workload/record.h"

namespace txn {

// New-order template for workload 2.
// Keys: [D, S1, S2, S3]
//   D  — district: increment next_o_id
//   S1-S3 — supply: decrement qty, increment ytd and order_cnt
// key_builder must be injected in main.cpp.
inline WorkloadTemplate MakeW2NewOrderTemplate() {
    return {
        "new_order",
        4,
        nullptr, // key_builder injected in main.cpp
        [](TransactionManager& mgr, const std::vector<std::string>& keys) -> CommitResult {
            auto txn = mgr.Begin("new_order", keys);

            // District: increment next_o_id
            auto val_d = mgr.Read(txn, keys[0]);
            Record rec_d = val_d.has_value() ? DeserializeRecord(val_d.value()) : Record{};
            SetIntField(rec_d, "next_o_id", GetIntField(rec_d, "next_o_id") + 1);
            mgr.Write(txn, keys[0], SerializeRecord(rec_d));

            // 3 supply records: decrement qty, increment ytd and order_cnt
            for (int i = 1; i <= 3; i++) {
                auto val_s = mgr.Read(txn, keys[i]);
                Record rec_s = val_s.has_value() ? DeserializeRecord(val_s.value()) : Record{};
                SetIntField(rec_s, "qty",       GetIntField(rec_s, "qty")       - 1);
                SetIntField(rec_s, "ytd",       GetIntField(rec_s, "ytd")       + 1);
                SetIntField(rec_s, "order_cnt", GetIntField(rec_s, "order_cnt") + 1);
                mgr.Write(txn, keys[i], SerializeRecord(rec_s));
            }

            return mgr.Commit(txn);
        }
    };
}

// Payment template for workload 2.
// Keys: [W, D, C]
//   W — warehouse: ytd += 5
//   D — district:  ytd += 5
//   C — customer:  balance -= 5, ytd_payment += 5, payment_cnt += 1
// key_builder must be injected in main.cpp.
inline WorkloadTemplate MakeW2PaymentTemplate() {
    return {
        "payment",
        3,
        nullptr, // key_builder injected in main.cpp
        [](TransactionManager& mgr, const std::vector<std::string>& keys) -> CommitResult {
            auto txn = mgr.Begin("payment", keys);

            // Warehouse: ytd += 5
            auto val_w = mgr.Read(txn, keys[0]);
            Record rec_w = val_w.has_value() ? DeserializeRecord(val_w.value()) : Record{};
            SetIntField(rec_w, "ytd", GetIntField(rec_w, "ytd") + 5);
            mgr.Write(txn, keys[0], SerializeRecord(rec_w));

            // District: ytd += 5
            auto val_d = mgr.Read(txn, keys[1]);
            Record rec_d = val_d.has_value() ? DeserializeRecord(val_d.value()) : Record{};
            SetIntField(rec_d, "ytd", GetIntField(rec_d, "ytd") + 5);
            mgr.Write(txn, keys[1], SerializeRecord(rec_d));

            // Customer: balance -= 5, ytd_payment += 5, payment_cnt += 1
            auto val_c = mgr.Read(txn, keys[2]);
            Record rec_c = val_c.has_value() ? DeserializeRecord(val_c.value()) : Record{};
            SetIntField(rec_c, "balance",     GetIntField(rec_c, "balance")     - 5);
            SetIntField(rec_c, "ytd_payment", GetIntField(rec_c, "ytd_payment") + 5);
            SetIntField(rec_c, "payment_cnt", GetIntField(rec_c, "payment_cnt") + 1);
            mgr.Write(txn, keys[2], SerializeRecord(rec_c));

            return mgr.Commit(txn);
        }
    };
}

} // namespace txn

#endif // WORKLOAD2_TEMPLATES_H
