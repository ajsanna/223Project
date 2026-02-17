#ifndef KEY_SELECTOR_H
#define KEY_SELECTOR_H

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

} // namespace txn

#endif // KEY_SELECTOR_H
