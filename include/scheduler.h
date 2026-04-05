#pragma once
#include "request.h"
#include <vector>
#include <queue>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <functional>
#include <unordered_map>

enum class SchedulingPolicy {
    FCFS,
    SJF,
    PRIORITY,
    ROUND_ROBIN,
    WFQ
};

// Returns a canonical short name for a policy.
std::string policyName(SchedulingPolicy p);

// Parses a policy name string; throws std::invalid_argument on failure.
SchedulingPolicy parsePolicy(const std::string& s);

// Policy-driven, thread-safe job dispatcher.
// Supports FCFS, SJF (shortest size), Priority (1=highest), preemptive
// Round Robin, and Weighted Fair Queuing with virtual-time accounting.
class Scheduler {
public:
    explicit Scheduler(SchedulingPolicy policy,
                       double rr_quantum_mb  = 6.25,    // ~50 ms @ 100 MB/s
                       double bandwidth_mbps = 100.0);

    // Add a job to the scheduler's internal queue.
    void addJob(TransferJob job);

    // Retrieve the next job according to the active policy.
    // Blocks until a job is available or the scheduler is shut down.
    // Returns nullopt when shut down and queue is empty.
    std::optional<TransferJob> getNextJob();

    // Signal that no more jobs will be added.
    void shutdown();

    bool isShutdown() const { return shutdown_; }
    SchedulingPolicy policy() const { return policy_; }

    // Number of jobs currently waiting.
    size_t pendingCount() const;

    double rrQuantumMb() const { return rr_quantum_mb_; }

    // Re-insert a preempted Round Robin job.
    void requeue(TransferJob job);

private:
    SchedulingPolicy policy_;
    double rr_quantum_mb_;
    double bandwidth_mbps_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool shutdown_ = false;

    // Storage per policy (only one is active at a time).
    std::deque<TransferJob> fifo_;          // FCFS / RR

    // SJF: min-heap by size_mb
    struct SJFCmp {
        bool operator()(const TransferJob& a, const TransferJob& b) const {
            return a.size_mb > b.size_mb;
        }
    };
    std::priority_queue<TransferJob, std::vector<TransferJob>, SJFCmp> sjf_pq_;

    // Priority: min-heap by priority field (1 = highest)
    struct PriCmp {
        bool operator()(const TransferJob& a, const TransferJob& b) const {
            return a.priority > b.priority;
        }
    };
    std::priority_queue<TransferJob, std::vector<TransferJob>, PriCmp> pri_pq_;

    // WFQ: min-heap by virtual_finish_time
    struct WFQCmp {
        bool operator()(const TransferJob& a, const TransferJob& b) const {
            return a.virtual_finish_time > b.virtual_finish_time;
        }
    };
    std::priority_queue<TransferJob, std::vector<TransferJob>, WFQCmp> wfq_pq_;

    double virtual_time_ = 0.0;

    // WFQ weight per user group (user_id % 3 → group 0/1/2).
    static constexpr double WFQ_WEIGHTS[3] = { 1.0, 2.0, 3.0 };
};
