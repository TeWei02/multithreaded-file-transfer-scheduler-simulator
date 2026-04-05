#include "metrics.h"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <cmath>

void MetricsCollector::recordCompletion(const TransferJob& job) {
    std::unique_lock<std::mutex> lock(mutex_);
    jobs_.push_back(job);
}

void MetricsCollector::clear() {
    std::unique_lock<std::mutex> lock(mutex_);
    jobs_.clear();
}

SimStats MetricsCollector::computeStats(const std::string& policy,
                                        double sim_duration_ms,
                                        double total_service_ms,
                                        int    num_workers) const {
    std::unique_lock<std::mutex> lock(mutex_);
    SimStats s;
    s.policy_name = policy;
    s.completed   = static_cast<int>(jobs_.size());
    s.sim_duration_ms = sim_duration_ms;
    if (s.completed == 0) return s;

    double sum_wait = 0.0, sum_tat = 0.0, sum_resp = 0.0;
    for (const auto& j : jobs_) {
        sum_wait += j.start_ms      - j.arrival_ms;
        sum_tat  += j.completion_ms - j.arrival_ms;
        sum_resp += j.first_start_ms - j.arrival_ms;
    }
    s.avg_wait_ms     = sum_wait / s.completed;
    s.avg_tat_ms      = sum_tat  / s.completed;
    s.avg_response_ms = sum_resp / s.completed;

    if (sim_duration_ms > 0.0)
        s.throughput = s.completed / (sim_duration_ms / 1000.0); // jobs/s

    if (sim_duration_ms > 0.0 && num_workers > 0)
        s.utilization = total_service_ms / (static_cast<double>(num_workers) * sim_duration_ms);

    // Jain's Fairness Index over per-user throughput.
    std::unordered_map<int, double> user_bytes;
    for (const auto& j : jobs_)
        user_bytes[j.user_id] += j.size_mb;

    std::vector<double> vals;
    vals.reserve(user_bytes.size());
    for (auto& [uid, mb] : user_bytes)
        vals.push_back(mb);

    if (!vals.empty()) {
        double sum_x  = 0.0, sum_x2 = 0.0;
        for (double x : vals) { sum_x += x; sum_x2 += x * x; }
        double n = static_cast<double>(vals.size());
        s.jains_fairness = (sum_x2 > 0.0) ? (sum_x * sum_x) / (n * sum_x2) : 1.0;
    }
    return s;
}

void MetricsCollector::writeResultsCsv(const std::string& path,
                                       const std::string& policy) const {
    std::unique_lock<std::mutex> lock(mutex_);
    std::ofstream f(path, std::ios::app);
    if (!f) return;
    for (const auto& j : jobs_) {
        f << policy << ","
          << j.id << "," << j.user_id << ","
          << std::fixed << std::setprecision(2) << j.size_mb << ","
          << j.directionStr() << ","
          << j.priority << ","
          << j.arrival_ms << ","
          << j.start_ms   << ","
          << j.completion_ms << ","
          << (j.start_ms - j.arrival_ms) << ","
          << (j.completion_ms - j.arrival_ms) << "\n";
    }
}

void MetricsCollector::writePolicyComparisonCsv(const std::string& path,
                                                const SimStats& s,
                                                bool write_header) const {
    std::ofstream f(path, std::ios::app);
    if (!f) return;
    if (write_header)
        f << "policy,avg_wait_ms,avg_tat_ms,throughput,utilization,fairness_index\n";
    f << std::fixed << std::setprecision(3)
      << s.policy_name << ","
      << s.avg_wait_ms << ","
      << s.avg_tat_ms  << ","
      << s.throughput  << ","
      << s.utilization << ","
      << s.jains_fairness << "\n";
}
