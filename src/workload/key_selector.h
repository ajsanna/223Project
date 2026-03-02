#ifndef KEY_SELECTOR_H
#define KEY_SELECTOR_H

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <random>
#include <set>

namespace txn {

struct ContentionConfig {
    int total_keys = 1000;
    int hotset_size = 10;
    double hotset_probability = 0.5;
};

class KeySelector {
public:
    explicit KeySelector(const ContentionConfig& config, std::mt19937& rng)
        : config_(config), rng_(rng),
          prob_dist_(0.0, 1.0),
          hot_dist_(0, config.hotset_size - 1),
          full_dist_(0, config.total_keys - 1) {}

    std::string SelectKey() {
        int idx;
        if (prob_dist_(rng_) < config_.hotset_probability) {
            idx = hot_dist_(rng_);
        } else {
            idx = full_dist_(rng_);
        }
        return "account_" + std::to_string(idx);
    }

    std::vector<std::string> SelectDistinctKeys(int n) {
        std::set<std::string> keys;
        while (static_cast<int>(keys.size()) < n) {
            keys.insert(SelectKey());
        }
        return std::vector<std::string>(keys.begin(), keys.end());
    }

private:
    ContentionConfig config_;
    std::mt19937& rng_;
    std::uniform_real_distribution<double> prob_dist_;
    std::uniform_int_distribution<int> hot_dist_;
    std::uniform_int_distribution<int> full_dist_;
};

// Per-domain key selector for workloads with multiple key types (e.g., workload 2).
// Thread-safe: rng is passed per-call, no shared mutable state.
class MultiDomainKeySelector {
public:
    struct DomainConfig {
        std::vector<std::string> all_keys;
        int hotset_size;
        double hotset_probability;
    };

    explicit MultiDomainKeySelector(std::map<std::string, DomainConfig> domains)
        : domains_(std::move(domains)) {}

    // Select one key from the named domain using hotset probability.
    std::string SelectFromDomain(const std::string& domain_name, std::mt19937& rng) {
        auto it = domains_.find(domain_name);
        if (it == domains_.end() || it->second.all_keys.empty()) return "";

        const auto& cfg = it->second;
        int n = static_cast<int>(cfg.all_keys.size());

        std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
        int idx;
        if (cfg.hotset_size > 0 && prob_dist(rng) < cfg.hotset_probability) {
            int hot_max = std::min(cfg.hotset_size, n) - 1;
            std::uniform_int_distribution<int> hot_dist(0, hot_max);
            idx = hot_dist(rng);
        } else {
            std::uniform_int_distribution<int> full_dist(0, n - 1);
            idx = full_dist(rng);
        }
        return cfg.all_keys[idx];
    }

private:
    std::map<std::string, DomainConfig> domains_;
};

} // namespace txn

#endif // KEY_SELECTOR_H
