#include "packet_scheduler.h"
#include <algorithm>
#include <stdexcept>

// ── DRR ──────────────────────────────────────────────────────────────────────

DRRScheduler::DRRScheduler(double quantum_mb) : quantum_mb_(quantum_mb) {}

void DRRScheduler::enqueue(Packet pkt) {
    std::unique_lock<std::mutex> lock(mutex_);
    int fid = pkt.flow_id;
    bool new_flow = queues_[fid].empty();
    queues_[fid].push_back(std::move(pkt));
    if (new_flow) {
        round_robin_order_.push_back(fid);
        deficit_[fid] = 0.0;
    }
}

std::optional<Packet> DRRScheduler::dequeue() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (round_robin_order_.empty()) return std::nullopt;

    // Try each flow in round-robin order until we find one to serve.
    for (size_t attempt = 0; attempt < round_robin_order_.size(); ++attempt) {
        int fid = round_robin_order_.front();
        round_robin_order_.pop_front();

        auto& q = queues_[fid];
        if (q.empty()) {
            deficit_.erase(fid);
            continue; // skip empty queues
        }

        deficit_[fid] += quantum_mb_;

        while (!q.empty() && q.front().size_mb <= deficit_[fid]) {
            deficit_[fid] -= q.front().size_mb;
            Packet pkt = std::move(q.front());
            q.pop_front();
            if (q.empty()) {
                // Don't re-enqueue empty flows; deficit reset on re-entry.
            } else {
                round_robin_order_.push_back(fid);
            }
            return pkt;
        }
        // Still has packets but deficit exhausted; re-queue flow.
        if (!q.empty())
            round_robin_order_.push_back(fid);
    }
    return std::nullopt;
}

bool DRRScheduler::empty() const {
    std::unique_lock<std::mutex> lock(mutex_);
    for (auto& [fid, q] : queues_)
        if (!q.empty()) return false;
    return true;
}

size_t DRRScheduler::totalPackets() const {
    std::unique_lock<std::mutex> lock(mutex_);
    size_t total = 0;
    for (auto& [fid, q] : queues_) total += q.size();
    return total;
}

// ── HTB ──────────────────────────────────────────────────────────────────────

HTBScheduler::HTBScheduler() {
    for (int c = 0; c < 3; ++c) {
        HTBClass cls;
        cls.rate_mbps = CLASS_RATES[c];
        cls.burst_mb  = cls.rate_mbps * 0.2; // 200 ms burst
        cls.tokens    = cls.burst_mb;
        classes_[c]   = cls;
    }
}

void HTBScheduler::enqueue(Packet pkt) {
    std::unique_lock<std::mutex> lock(mutex_);
    int cls = pkt.flow_id % 3;
    queues_[cls].push_back(std::move(pkt));
}

std::optional<Packet> HTBScheduler::dequeue(double elapsed_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    // Refill tokens for each class.
    for (auto& [cid, cls] : classes_) {
        double refill = (cls.rate_mbps * elapsed_ms) / 1000.0;
        cls.tokens = std::min(cls.tokens + refill, cls.burst_mb);
    }
    // Serve highest-priority class (id 0) with available tokens first.
    for (int c = 0; c < 3; ++c) {
        auto& q = queues_[c];
        auto& cls = classes_[c];
        if (!q.empty() && q.front().size_mb <= cls.tokens) {
            cls.tokens -= q.front().size_mb;
            Packet pkt = std::move(q.front());
            q.pop_front();
            return pkt;
        }
    }
    // No class has tokens; serve best-effort (class 2) to avoid starvation.
    for (int c = 2; c >= 0; --c) {
        auto& q = queues_[c];
        if (!q.empty()) {
            Packet pkt = std::move(q.front());
            q.pop_front();
            return pkt;
        }
    }
    return std::nullopt;
}

bool HTBScheduler::empty() const {
    std::unique_lock<std::mutex> lock(mutex_);
    for (auto& [cid, q] : queues_)
        if (!q.empty()) return false;
    return true;
}
