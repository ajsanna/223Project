#include <iostream>
#include <string>
#include <map>
#include "database/database.h"
#include "concurrency/occ_manager.h"
#include "workload/workload_template.h"
#include "workload/workload_executor.h"
#include "metrics/metrics.h"

using namespace txn;

struct CLIArgs {
    int threads = 4;
    int txns_per_thread = 100;
    int total_keys = 1000;
    int hotset_size = 10;
    double hotset_prob = 0.5;
    std::string protocol = "occ";
    std::string db_path = "transaction_db";
};

CLIArgs ParseArgs(int argc, char* argv[]) {
    CLIArgs args;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--threads" && i + 1 < argc) {
            args.threads = std::stoi(argv[++i]);
        } else if (arg == "--txns-per-thread" && i + 1 < argc) {
            args.txns_per_thread = std::stoi(argv[++i]);
        } else if (arg == "--total-keys" && i + 1 < argc) {
            args.total_keys = std::stoi(argv[++i]);
        } else if (arg == "--hotset-size" && i + 1 < argc) {
            args.hotset_size = std::stoi(argv[++i]);
        } else if (arg == "--hotset-prob" && i + 1 < argc) {
            args.hotset_prob = std::stod(argv[++i]);
        } else if (arg == "--protocol" && i + 1 < argc) {
            args.protocol = argv[++i];
        } else if (arg == "--db-path" && i + 1 < argc) {
            args.db_path = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: transaction_system [options]\n"
                      << "  --threads N          Number of worker threads (default: 4)\n"
                      << "  --txns-per-thread N  Transactions per thread (default: 100)\n"
                      << "  --total-keys N       Total number of keys (default: 1000)\n"
                      << "  --hotset-size N      Size of hot key set (default: 10)\n"
                      << "  --hotset-prob P      Probability of picking hot key (default: 0.5)\n"
                      << "  --protocol P         Concurrency protocol: occ (default: occ)\n"
                      << "  --db-path PATH       Database directory path (default: transaction_db)\n";
            exit(0);
        }
    }
    return args;
}

int main(int argc, char* argv[]) {
    CLIArgs args = ParseArgs(argc, argv);

    std::cout << "Transaction Processing System" << std::endl;
    std::cout << "=============================" << std::endl;
    std::cout << "Protocol:        " << args.protocol << std::endl;
    std::cout << "Threads:         " << args.threads << std::endl;
    std::cout << "Txns/thread:     " << args.txns_per_thread << std::endl;
    std::cout << "Total keys:      " << args.total_keys << std::endl;
    std::cout << "Hotset size:     " << args.hotset_size << std::endl;
    std::cout << "Hotset prob:     " << args.hotset_prob << std::endl;
    std::cout << "DB path:         " << args.db_path << std::endl;
    std::cout << std::endl;

    // Open database
    Database db;
    if (!db.Open(args.db_path)) {
        std::cerr << "Failed to open database" << std::endl;
        return 1;
    }

    // Initialize accounts
    std::map<std::string, std::string> initial_data;
    for (int i = 0; i < args.total_keys; i++) {
        initial_data["account_" + std::to_string(i)] = std::to_string(1000);
    }
    db.InitializeWithData(initial_data);

    // Create concurrency manager
    if (args.protocol != "occ") {
        std::cerr << "Unknown protocol: " << args.protocol << std::endl;
        return 1;
    }
    OCCManager mgr(db);

    // Set up workload templates
    std::vector<WorkloadTemplate> templates;
    templates.push_back(MakeTransferTemplate());
    templates.push_back(MakeBalanceCheckTemplate());
    templates.push_back(MakeWriteHeavyTemplate(4));

    // Configure executor
    ExecutorConfig exec_config;
    exec_config.num_threads = args.threads;
    exec_config.txns_per_thread = args.txns_per_thread;
    exec_config.contention = {args.total_keys, args.hotset_size, args.hotset_prob};
    exec_config.templates = templates;
    exec_config.retry_backoff_base_us = 100;

    // Run workload
    MetricsCollector metrics;
    WorkloadExecutor executor(mgr, metrics, exec_config);

    std::cout << "Running workload..." << std::endl;
    executor.Run();

    // Print results
    metrics.PrintReport(executor.ElapsedSeconds());

    // Verify balance conservation
    // Transfers are zero-sum; balance_checks are read-only; write_heavy adds +1 per key touched
    long long total_balance = 0;
    for (int i = 0; i < args.total_keys; i++) {
        auto val = db.Get("account_" + std::to_string(i));
        if (val.has_value()) {
            total_balance += std::stoi(val.value());
        }
    }
    long long initial_total = static_cast<long long>(args.total_keys) * 1000;
    long long net_added = total_balance - initial_total;
    std::cout << "\nBalance check:\n";
    std::cout << "  Initial total:  " << initial_total << "\n";
    std::cout << "  Final total:    " << total_balance << "\n";
    std::cout << "  Net change:     " << net_added
              << " (from write_heavy increments)\n";

    db.Close();
    return 0;
}
