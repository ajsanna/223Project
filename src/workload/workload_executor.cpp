#include "workload/workload_executor.h"
#include <thread>
#include <random>
#include <chrono>

namespace txn {

WorkloadExecutor::WorkloadExecutor(TransactionManager& mgr, MetricsCollector& metrics,
                                   const ExecutorConfig& config)
    : mgr_(mgr), metrics_(metrics), config_(config) {}

void WorkloadExecutor::Run() {
    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(config_.num_threads);

    for (int i = 0; i < config_.num_threads; i++) {
        threads.emplace_back(&WorkloadExecutor::WorkerThread, this, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();
    elapsed_s_ = std::chrono::duration<double>(end - start).count();
}

double WorkloadExecutor::ElapsedSeconds() const {
    return elapsed_s_;
}

void WorkloadExecutor::WorkerThread(int thread_id) {
    std::mt19937 rng(thread_id + std::chrono::steady_clock::now().time_since_epoch().count());
    KeySelector key_selector(config_.contention, rng);
    std::uniform_int_distribution<int> template_dist(0, config_.templates.size() - 1);

    for (int i = 0; i < config_.txns_per_thread; i++) {
        // Pick a random template
        auto& tmpl = config_.templates[template_dist(rng)];
        auto keys = key_selector.SelectDistinctKeys(tmpl.num_input_keys);

        auto wall_start = std::chrono::steady_clock::now();
        int retries = 0;

        while (true) {
            auto result = tmpl.execute(mgr_, keys);

            if (result.success) {
                auto wall_end = std::chrono::steady_clock::now();
                double latency_us = std::chrono::duration<double, std::micro>(
                    wall_end - wall_start).count();
                metrics_.RecordCommit(tmpl.name, latency_us);
                break;
            } else {
                metrics_.RecordAbort(tmpl.name);
                retries++;

                // Exponential backoff with jitter
                int backoff_us = config_.retry_backoff_base_us * (1 << std::min(retries, 10));
                std::uniform_int_distribution<int> jitter(0, backoff_us);
                std::this_thread::sleep_for(std::chrono::microseconds(backoff_us + jitter(rng)));
            }
        }
    }
}

} // namespace txn
