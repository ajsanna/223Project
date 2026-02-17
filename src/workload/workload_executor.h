#ifndef WORKLOAD_EXECUTOR_H
#define WORKLOAD_EXECUTOR_H

#include <vector>
#include <cstdint>
#include "workload/workload_template.h"
#include "workload/key_selector.h"
#include "concurrency/transaction_manager.h"
#include "metrics/metrics.h"

namespace txn {

struct ExecutorConfig {
    int num_threads = 4;
    int txns_per_thread = 100;
    ContentionConfig contention;
    std::vector<WorkloadTemplate> templates;
    int retry_backoff_base_us = 100;
};

class WorkloadExecutor {
public:
    WorkloadExecutor(TransactionManager& mgr, MetricsCollector& metrics,
                     const ExecutorConfig& config);

    void Run();
    double ElapsedSeconds() const;

private:
    void WorkerThread(int thread_id);

    TransactionManager& mgr_;
    MetricsCollector& metrics_;
    ExecutorConfig config_;
    double elapsed_s_ = 0.0;
};

} // namespace txn

#endif // WORKLOAD_EXECUTOR_H
