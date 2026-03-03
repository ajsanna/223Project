#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <vector>

#include "database/database.h"
#include "concurrency/occ_manager.h"
#include "concurrency/twopl_manager.h"
#include "workload/workload_template.h"
#include "workload/workload_executor.h"
#include "workload/input_parser.h"
#include "workload/key_selector.h"
#include "workload/workload1_templates.h"
#include "workload/workload2_templates.h"
#include "workload/record.h"
#include "metrics/metrics.h"

using namespace txn;

struct CLIArgs {
    int threads          = 4;
    int txns_per_thread  = 100;
    int hotset_size      = 10;
    double hotset_prob   = 0.5;
    std::string protocol = "occ";
    std::string db_path  = "";         // auto-derived if empty
    int workload         = 1;
    std::string input_file     = "";   // auto-derived if empty
    std::string csv_output     = "";
    std::string dump_latencies = "";
};

CLIArgs ParseArgs(int argc, char* argv[]) {
    CLIArgs args;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--threads" && i + 1 < argc) {
            args.threads = std::stoi(argv[++i]);
        } else if (arg == "--txns-per-thread" && i + 1 < argc) {
            args.txns_per_thread = std::stoi(argv[++i]);
        } else if (arg == "--hotset-size" && i + 1 < argc) {
            args.hotset_size = std::stoi(argv[++i]);
        } else if (arg == "--hotset-prob" && i + 1 < argc) {
            args.hotset_prob = std::stod(argv[++i]);
        } else if (arg == "--protocol" && i + 1 < argc) {
            args.protocol = argv[++i];
        } else if (arg == "--db-path" && i + 1 < argc) {
            args.db_path = argv[++i];
        } else if (arg == "--workload" && i + 1 < argc) {
            args.workload = std::stoi(argv[++i]);
        } else if (arg == "--input-file" && i + 1 < argc) {
            args.input_file = argv[++i];
        } else if (arg == "--csv-output" && i + 1 < argc) {
            args.csv_output = argv[++i];
        } else if (arg == "--dump-latencies" && i + 1 < argc) {
            args.dump_latencies = argv[++i];
        } else if (arg == "--help") {
            std::cout
                << "Usage: transaction_system [options]\n"
                << "  --workload N           Workload: 1 (bank transfer) or 2 (TPC-C-like)\n"
                << "  --threads N            Worker threads (default: 4)\n"
                << "  --txns-per-thread N    Transactions per thread (default: 100)\n"
                << "  --hotset-size N        Hot key set size (default: 10)\n"
                << "  --hotset-prob P        Hot key probability (default: 0.5)\n"
                << "  --protocol P           occ | 2pl (default: occ)\n"
                << "  --db-path PATH         Database directory (auto if omitted)\n"
                << "  --input-file PATH      Input file (auto if omitted)\n"
                << "  --csv-output PATH      Append results row to CSV\n"
                << "  --dump-latencies PATH  Dump raw latency samples to CSV\n";
            exit(0);
        }
    }
    return args;
}

int main(int argc, char* argv[]) {
    CLIArgs args = ParseArgs(argc, argv);

    // Auto-derive paths
    if (args.db_path.empty()) {
        args.db_path = "db_w" + std::to_string(args.workload) + "_" + args.protocol;
    }
    if (args.input_file.empty()) {
        args.input_file = "workloads/workload" + std::to_string(args.workload)
                        + "/input" + std::to_string(args.workload) + ".txt";
    }

    std::cout << "Transaction Processing System\n"
              << "=============================\n"
              << "Workload:        " << args.workload        << "\n"
              << "Protocol:        " << args.protocol        << "\n"
              << "Threads:         " << args.threads         << "\n"
              << "Txns/thread:     " << args.txns_per_thread << "\n"
              << "Hotset size:     " << args.hotset_size     << "\n"
              << "Hotset prob:     " << args.hotset_prob     << "\n"
              << "DB path:         " << args.db_path         << "\n"
              << "Input file:      " << args.input_file      << "\n\n";

    // Parse input file
    ParseResult parsed = ParseInputFile(args.input_file);

    // Open and initialize database
    Database db;
    if (!db.Open(args.db_path)) {
        std::cerr << "Failed to open database: " << args.db_path << "\n";
        return 1;
    }
    db.InitializeWithData(parsed.initial_data);

    std::cout << "Loaded " << parsed.initial_data.size() << " records\n";

    // Create concurrency manager
    std::unique_ptr<TransactionManager> mgr_ptr;
    if (args.protocol == "occ") {
        mgr_ptr = std::make_unique<OCCManager>(db);
    } else if (args.protocol == "2pl") {
        mgr_ptr = std::make_unique<TwoPLManager>(db);
    } else {
        std::cerr << "Unknown protocol: " << args.protocol << "\n";
        return 1;
    }
    TransactionManager& mgr = *mgr_ptr;

    // Build workload templates with injected key_builder lambdas
    std::vector<WorkloadTemplate> templates;

    if (args.workload == 1) {
        auto account_keys = parsed.account_keys;
        int  hotset_size  = args.hotset_size;
        double hotset_prob = args.hotset_prob;

        auto tmpl = MakeW1TransferTemplate();
        tmpl.key_builder = [account_keys, hotset_size, hotset_prob]
                           (std::mt19937& rng) -> std::vector<std::string> {
            int n       = static_cast<int>(account_keys.size());
            int hot_max = std::min(hotset_size, n) - 1;
            std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
            std::uniform_int_distribution<int>     hot_dist(0, std::max(0, hot_max));
            std::uniform_int_distribution<int>     all_dist(0, n - 1);

            std::set<int> used;
            std::vector<std::string> keys;
            while (static_cast<int>(keys.size()) < 2) {
                int idx = (prob_dist(rng) < hotset_prob) ? hot_dist(rng) : all_dist(rng);
                if (used.find(idx) == used.end()) {
                    used.insert(idx);
                    keys.push_back(account_keys[idx]);
                }
            }
            return keys;
        };
        templates.push_back(std::move(tmpl));

    } else if (args.workload == 2) {
        int    hotset_size = args.hotset_size;
        double hotset_prob = args.hotset_prob;

        // Scale hotset size proportionally to each domain's size vs. workload-1's 500 keys.
        auto make_domain = [&](const std::vector<std::string>& keys)
                -> MultiDomainKeySelector::DomainConfig {
            int domain_size  = static_cast<int>(keys.size());
            int scaled_hot   = std::max(1, domain_size * hotset_size / 500);
            return {keys, scaled_hot, hotset_prob};
        };

        auto selector = std::make_shared<MultiDomainKeySelector>(
            std::map<std::string, MultiDomainKeySelector::DomainConfig>{
                {"W", make_domain(parsed.warehouse_keys)},
                {"D", make_domain(parsed.district_keys)},
                {"S", make_domain(parsed.supply_keys)},
                {"C", make_domain(parsed.customer_keys)},
            });

        // new_order: keys = [D, S1, S2, S3] with 3 distinct supply keys
        auto tmpl_no = MakeW2NewOrderTemplate();
        tmpl_no.key_builder = [selector](std::mt19937& rng) -> std::vector<std::string> {
            std::vector<std::string> keys;
            keys.push_back(selector->SelectFromDomain("D", rng));
            std::set<std::string> used;
            while (static_cast<int>(used.size()) < 3) {
                used.insert(selector->SelectFromDomain("S", rng));
            }
            for (const auto& k : used) keys.push_back(k);
            return keys;
        };
        templates.push_back(std::move(tmpl_no));

        // payment: keys = [W, D, C]
        auto tmpl_pay = MakeW2PaymentTemplate();
        tmpl_pay.key_builder = [selector](std::mt19937& rng) -> std::vector<std::string> {
            return {
                selector->SelectFromDomain("W", rng),
                selector->SelectFromDomain("D", rng),
                selector->SelectFromDomain("C", rng),
            };
        };
        templates.push_back(std::move(tmpl_pay));

    } else {
        std::cerr << "Unknown workload: " << args.workload << "\n";
        return 1;
    }

    // Configure and run executor
    ExecutorConfig exec_config;
    exec_config.num_threads         = args.threads;
    exec_config.txns_per_thread     = args.txns_per_thread;
    exec_config.contention          = {static_cast<int>(parsed.initial_data.size()),
                                       args.hotset_size, args.hotset_prob};
    exec_config.templates           = templates;
    exec_config.retry_backoff_base_us = 100;

    MetricsCollector metrics;
    WorkloadExecutor executor(mgr, metrics, exec_config);

    std::cout << "Running workload...\n";
    executor.Run();

    double elapsed = executor.ElapsedSeconds();
    metrics.PrintReport(elapsed);

    // Optional CSV output
    if (!args.csv_output.empty()) {
        metrics.WriteCsvRow(args.csv_output, std::to_string(args.workload),
                            args.protocol, args.threads, args.hotset_prob, elapsed);
        std::cout << "Results appended to " << args.csv_output << "\n";
    }

    if (!args.dump_latencies.empty()) {
        metrics.DumpLatencies(args.dump_latencies, std::to_string(args.workload),
                              args.protocol, args.threads, args.hotset_prob);
        std::cout << "Latencies written to " << args.dump_latencies << "\n";
    }

    // Workload 1: verify zero-sum balance conservation
    if (args.workload == 1) {
        long long initial_total = 0;
        long long final_total   = 0;

        for (const auto& key : parsed.account_keys) {
            auto it = parsed.initial_data.find(key);
            if (it != parsed.initial_data.end()) {
                initial_total += GetIntField(DeserializeRecord(it->second), "balance");
            }
            auto val = db.Get(key);
            if (val.has_value()) {
                final_total += GetIntField(DeserializeRecord(val.value()), "balance");
            }
        }

        std::cout << "\nBalance conservation check:\n"
                  << "  Initial total:  " << initial_total << "\n"
                  << "  Final total:    " << final_total   << "\n"
                  << "  Difference:     " << (final_total - initial_total)
                  << " (should be 0)\n";
    }

    db.Close();
    return 0;
}
