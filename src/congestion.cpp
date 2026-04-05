#include "congestion.h"
#include <algorithm>

CongestionController::CongestionController(double max_cwnd, double loss_threshold_util)
    : max_cwnd_(max_cwnd), loss_threshold_util_(loss_threshold_util) {}

CongestionState& CongestionController::getState(int flow_id) {
    auto it = states_.find(flow_id);
    if (it == states_.end()) {
        states_[flow_id] = CongestionState{};
        return states_[flow_id];
    }
    return it->second;
}

bool CongestionController::isCongested(double utilization) const {
    return utilization >= loss_threshold_util_;
}

double CongestionController::onSuccess(int flow_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto& s = getState(flow_id);
    if (s.cwnd < s.ssthresh) {
        // Slow start: exponential increase.
        s.cwnd = std::min(s.cwnd * 2.0, s.ssthresh);
    } else {
        // Congestion avoidance: additive increase.
        s.cwnd = std::min(s.cwnd + 1.0, max_cwnd_);
    }
    return s.cwnd / max_cwnd_;
}

double CongestionController::onCongestion(int flow_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto& s = getState(flow_id);
    // Multiplicative decrease.
    s.ssthresh = std::max(s.cwnd / 2.0, 1.0);
    s.cwnd     = 1.0; // restart from 1 MB window
    return s.cwnd;
}

double CongestionController::scalingFactor(int flow_id) const {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = states_.find(flow_id);
    if (it == states_.end()) return 1.0 / max_cwnd_;
    return it->second.cwnd / max_cwnd_;
}
