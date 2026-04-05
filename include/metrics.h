#pragma once
#include "request.h"
#include <vector>
#include <mutex>
#include <string>
#include <unordered_map>

// Aggregate statistics for one simulation run.
struct SimStats {
    std::string policy_name;
    int    completed      = 0;
    double avg_wait_ms    = 0.0;
    double avg_tat_ms     = 0.0;
    double avg_response_ms= 0.0;
    double throughput     = 0.0;    // jobs/s
    double utilization    = 0.0;    // 0..1
    double jains_fairness = 0.0;    // 0..1
    double sim_duration_ms= 0.0;
};

// Thread-safe collector: workers call recordCompletion() concurrently.
// After all workers finish, the owning thread calls computeStats().
class MetricsCollector {
public:
    void recordCompletion(const TransferJob& job);

    // Compute aggregate statistics given total simulation wall time and
    // the summed service time across all workers.
    SimStats computeStats(const std::string& policy,
                          double sim_duration_ms,
                          double total_service_ms,
                          int    num_workers) const;

    // Write per-job CSV (append mode so multiple policies accumulate).
    void writeResultsCsv(const std::string& path, const std::string& policy) const;

    // Write per-policy comparison CSV (one row per call).
    void writePolicyComparisonCsv(const std::string& path,
                                  const SimStats& stats,
                                  bool write_header) const;

    void clear();

    const std::vector<TransferJob>& completedJobs() const { return jobs_; }

private:
    mutable std::mutex mutex_;
    std::vector<TransferJob> jobs_;
};
