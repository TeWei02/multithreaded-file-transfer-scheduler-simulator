#pragma once
// Extension 2: AIMD (Additive Increase Multiplicative Decrease) congestion model.
// Models TCP-style congestion windows per flow to observe throughput collapse.

#include <unordered_map>
#include <mutex>

struct CongestionState {
    double cwnd     = 1.0;  // congestion window in MB
    double ssthresh = 16.0; // slow-start threshold
};

// Thread-safe per-flow congestion controller.
class CongestionController {
public:
    explicit CongestionController(double max_cwnd = 32.0,
                                  double loss_threshold_util = 0.80);

    // Call after a successful transfer quantum for a job (flow_id = job id).
    // Returns the effective bandwidth scaling factor (0 < f ≤ 1).
    double onSuccess(int flow_id);

    // Call when congestion is detected (e.g. server utilisation > threshold).
    // Returns the new cwnd.
    double onCongestion(int flow_id);

    // Effective bandwidth scaling for a given cwnd.
    double scalingFactor(int flow_id) const;

    double maxCwnd() const { return max_cwnd_; }

    // Returns true if current server utilisation triggers a congestion event.
    bool isCongested(double utilization) const;

private:
    double max_cwnd_;
    double loss_threshold_util_;
    mutable std::mutex mutex_;
    std::unordered_map<int, CongestionState> states_;

    CongestionState& getState(int flow_id);
};
