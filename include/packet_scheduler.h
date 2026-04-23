#pragma once
// Extension 3: Packet-level scheduling — Deficit Round Robin (DRR) and
// Hierarchical Token Bucket (HTB).

#include "request.h"
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <string>

// A single packet carved from a TransferJob.
struct Packet {
    int    job_id     = 0;
    int    flow_id    = 0;   // same as user_id for grouping
    double size_mb    = 1.0;
    int    seq        = 0;   // sequence number within job
    bool   last       = false; // last packet of the job
    // Back-reference for metric attribution.
    double job_arrival_ms = 0.0;
    double job_start_ms   = 0.0;
};

// ── Deficit Round Robin ───────────────────────────────────────────────────────
// Each flow gets a quantum of 'quantum_mb' deficit per round.
// Packets are served as long as deficit ≥ packet.size_mb.

class DRRScheduler {
public:
    explicit DRRScheduler(double quantum_mb = 2.0);

    void enqueue(Packet pkt);
    std::optional<Packet> dequeue();
    bool empty() const;
    size_t totalPackets() const;

private:
    double quantum_mb_;
    mutable std::mutex mutex_;

    std::deque<int> round_robin_order_; // flow ids in current round
    std::unordered_map<int, std::deque<Packet>> queues_;
    std::unordered_map<int, double> deficit_;
};

// ── Hierarchical Token Bucket ─────────────────────────────────────────────────
// Two-level HTB: root bucket → per-class leaf buckets.
// Classes are mapped from job.user_id % 3 (0=high, 1=medium, 2=low).

struct HTBClass {
    double rate_mbps  = 10.0; // sustained rate (MB/s)
    double burst_mb   = 4.0;  // burst size (MB)
    double tokens     = 4.0;  // current tokens
};

class HTBScheduler {
public:
    HTBScheduler();

    void enqueue(Packet pkt);
    // Returns the next dequeue-able packet; refills tokens based on elapsed
    // simulated time 'elapsed_ms' since the last call.
    std::optional<Packet> dequeue(double elapsed_ms);
    bool empty() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<int, HTBClass>         classes_;  // class_id → bucket
    std::unordered_map<int, std::deque<Packet>> queues_; // class_id → packets
    inline static constexpr double CLASS_RATES[3] = { 20.0, 10.0, 5.0 }; // MB/s
};
