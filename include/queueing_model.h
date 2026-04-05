#pragma once
// Extension 7: Formal performance modeling.
// Computes M/M/c and M/G/1 queuing model predictions and compares them
// against simulation results.

#include <string>

// Result from M/M/c (Erlang-C) analysis.
struct MMcResult {
    bool   valid         = false;
    double erlang_c      = 0.0;  // P(wait): probability a job waits
    double avg_wait_ms   = 0.0;  // mean waiting time in queue (ms)
    double avg_queue_len = 0.0;  // mean number in queue (Lq)
    double avg_sojourn_ms= 0.0;  // mean sojourn time = wait + service (ms)
    double utilization   = 0.0;  // ρ = λ / (c·μ)
};

// Result from M/G/1 (Pollaczek-Khinchine) analysis.
struct MG1Result {
    bool   valid         = false;
    double avg_wait_ms   = 0.0;
    double avg_queue_len = 0.0;
    double avg_sojourn_ms= 0.0;
};

// Compute M/M/c queuing model.
// lambda_ms: arrival rate in jobs/ms
// mu_ms:     service rate per server in jobs/ms
// c:         number of servers
MMcResult computeMMc(double lambda_ms, double mu_ms, int c);

// Compute M/G/1 queuing model using Pollaczek-Khinchine mean formula.
// lambda_ms: arrival rate in jobs/ms
// E_S_ms:    mean service time in ms
// E_S2_ms2:  second moment of service time in ms²
MG1Result computeMG1(double lambda_ms, double E_S_ms, double E_S2_ms2);

// Print a comparison table of simulated vs analytical results.
void printQueueingModelComparison(const MMcResult& mmc,
                                  const MG1Result& mg1,
                                  double sim_avg_wait_ms,
                                  double sim_avg_queue_len,
                                  double sim_avg_sojourn_ms);
