#ifndef METRICS_H
#define METRICS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace txn {

struct PerTypeStat {
    std::atomic<uint64_t> commits{0};
    std::atomic<uint64_t> aborts{0};
    std::mutex latency_mutex;
    std::vector<double> latencies_us;
};

class MetricsCollector {
public:
    void RecordCommit(const std::string& type, double latency_us);
    void RecordAbort(const std::string& type);

    double AbortPercentage(const std::string& type);
    double Throughput(double elapsed_s);
    double AvgResponseTime(const std::string& type);
    double Percentile(const std::string& type, double p);
    uint64_t TotalCommits();
    uint64_t TotalAborts();

    void PrintReport(double elapsed_s);

private:
    std::mutex map_mutex_;
    std::unordered_map<std::string, PerTypeStat> stats_;

    PerTypeStat& GetStat(const std::string& type);
};

} // namespace txn

#endif // METRICS_H
