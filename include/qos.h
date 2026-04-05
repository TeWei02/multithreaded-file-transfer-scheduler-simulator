#pragma once
// Extension 4: DiffServ QoS class model.
// Maps user groups to Per-Hop Behaviors: EF, AF1, Best Effort.

#include "request.h"
#include "metrics.h"
#include <string>
#include <vector>

enum class DiffServPHB { EF, AF1, BE };

std::string phbName(DiffServPHB phb);

// Returns the DiffServ PHB for a given user_id.
// Users 1-3  → EF  (Expedited Forwarding: strict priority, low latency)
// Users 4-6  → AF1 (Assured Forwarding class 1: guaranteed bandwidth)
// Users 7-10 → BE  (Best Effort: no guarantees)
DiffServPHB classifyUser(int user_id);

// Returns a scheduling priority adjustment based on PHB.
// EF → priority 1 (highest), AF1 → 5, BE → 9.
int phbPriority(DiffServPHB phb);

// Per-class aggregate statistics.
struct QoSClassStats {
    DiffServPHB phb;
    int    count       = 0;
    double avg_wait_ms = 0.0;
    double avg_tat_ms  = 0.0;
    double throughput  = 0.0; // jobs/s
};

// Compute per-PHB statistics from a list of completed jobs.
std::vector<QoSClassStats> computeQoSStats(const std::vector<TransferJob>& jobs,
                                           double sim_duration_ms);

// Print a DiffServ class comparison table.
void printQoSStats(const std::vector<QoSClassStats>& stats);
