#include "metrics/metrics.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cmath>

namespace txn {

PerTypeStat& MetricsCollector::GetStat(const std::string& type) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    return stats_[type];
}

void MetricsCollector::RecordCommit(const std::string& type, double latency_us) {
    auto& stat = GetStat(type);
    stat.commits.fetch_add(1);
    std::lock_guard<std::mutex> lock(stat.latency_mutex);
    stat.latencies_us.push_back(latency_us);
}

void MetricsCollector::RecordAbort(const std::string& type) {
    auto& stat = GetStat(type);
    stat.aborts.fetch_add(1);
}

double MetricsCollector::AbortPercentage(const std::string& type) {
    auto& stat = GetStat(type);
    uint64_t c = stat.commits.load();
    uint64_t a = stat.aborts.load();
    uint64_t total = c + a;
    if (total == 0) return 0.0;
    return 100.0 * a / total;
}

double MetricsCollector::Throughput(double elapsed_s) {
    if (elapsed_s <= 0.0) return 0.0;
    return TotalCommits() / elapsed_s;
}

double MetricsCollector::AvgResponseTime(const std::string& type) {
    auto& stat = GetStat(type);
    std::lock_guard<std::mutex> lock(stat.latency_mutex);
    if (stat.latencies_us.empty()) return 0.0;
    double sum = 0.0;
    for (double v : stat.latencies_us) sum += v;
    return sum / stat.latencies_us.size();
}

double MetricsCollector::Percentile(const std::string& type, double p) {
    auto& stat = GetStat(type);
    std::lock_guard<std::mutex> lock(stat.latency_mutex);
    if (stat.latencies_us.empty()) return 0.0;

    std::vector<double> sorted = stat.latencies_us;
    std::sort(sorted.begin(), sorted.end());

    double index = (p / 100.0) * (sorted.size() - 1);
    size_t lo = static_cast<size_t>(std::floor(index));
    size_t hi = static_cast<size_t>(std::ceil(index));
    if (lo == hi) return sorted[lo];
    double frac = index - lo;
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

uint64_t MetricsCollector::TotalCommits() {
    std::lock_guard<std::mutex> lock(map_mutex_);
    uint64_t total = 0;
    for (auto& [_, stat] : stats_) {
        total += stat.commits.load();
    }
    return total;
}

uint64_t MetricsCollector::TotalAborts() {
    std::lock_guard<std::mutex> lock(map_mutex_);
    uint64_t total = 0;
    for (auto& [_, stat] : stats_) {
        total += stat.aborts.load();
    }
    return total;
}

namespace {

double ComputeAbortPct(PerTypeStat& stat) {
    uint64_t c = stat.commits.load();
    uint64_t a = stat.aborts.load();
    uint64_t total = c + a;
    if (total == 0) return 0.0;
    return 100.0 * a / total;
}

double ComputeAvgLatency(PerTypeStat& stat) {
    std::lock_guard<std::mutex> lock(stat.latency_mutex);
    if (stat.latencies_us.empty()) return 0.0;
    double sum = 0.0;
    for (double v : stat.latencies_us) sum += v;
    return sum / stat.latencies_us.size();
}

double ComputePercentile(PerTypeStat& stat, double p) {
    std::lock_guard<std::mutex> lock(stat.latency_mutex);
    if (stat.latencies_us.empty()) return 0.0;
    std::vector<double> sorted = stat.latencies_us;
    std::sort(sorted.begin(), sorted.end());
    double index = (p / 100.0) * (sorted.size() - 1);
    size_t lo = static_cast<size_t>(std::floor(index));
    size_t hi = static_cast<size_t>(std::ceil(index));
    if (lo == hi) return sorted[lo];
    double frac = index - lo;
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

} // anonymous namespace

void MetricsCollector::PrintReport(double elapsed_s) {
    // Gather totals without holding map_mutex_ across the whole function
    uint64_t total_commits = TotalCommits();
    uint64_t total_aborts = TotalAborts();
    double throughput = (elapsed_s > 0.0) ? total_commits / elapsed_s : 0.0;

    std::cout << "\n========== Performance Report ==========\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Elapsed time:    " << elapsed_s << " s\n";
    std::cout << "Total commits:   " << total_commits << "\n";
    std::cout << "Total aborts:    " << total_aborts << "\n";
    std::cout << "Throughput:      " << throughput << " txn/s\n";

    uint64_t total_all = total_commits + total_aborts;
    if (total_all > 0) {
        std::cout << "Overall abort %: " << (100.0 * total_aborts / total_all) << "%\n";
    }

    std::cout << "\n--- Per-Type Breakdown ---\n";
    std::lock_guard<std::mutex> lock(map_mutex_);
    for (auto& [type, stat] : stats_) {
        std::cout << "\n  [" << type << "]\n";
        std::cout << "    Commits:       " << stat.commits.load() << "\n";
        std::cout << "    Aborts:        " << stat.aborts.load() << "\n";
        std::cout << "    Abort %:       " << ComputeAbortPct(stat) << "%\n";
        std::cout << "    Avg latency:   " << ComputeAvgLatency(stat) << " us\n";
        std::cout << "    P50 latency:   " << ComputePercentile(stat, 50) << " us\n";
        std::cout << "    P90 latency:   " << ComputePercentile(stat, 90) << " us\n";
        std::cout << "    P99 latency:   " << ComputePercentile(stat, 99) << " us\n";
    }
    std::cout << "========================================\n";
}

} // namespace txn
