#include "queueing_model.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <limits>

// ── M/M/c (Erlang-C) ─────────────────────────────────────────────────────────

MMcResult computeMMc(double lambda_ms, double mu_ms, int c) {
    MMcResult r;
    if (lambda_ms <= 0.0 || mu_ms <= 0.0 || c <= 0) return r;

    double rho = lambda_ms / (c * mu_ms); // server utilisation
    if (rho >= 1.0) return r;             // system unstable

    double a = lambda_ms / mu_ms; // offered load (Erlangs)

    // P0: probability that all servers are idle.
    // P0 = 1 / [ sum_{k=0}^{c-1} a^k/k!  +  a^c/c! * 1/(1-rho) ]
    double sum = 0.0;
    double fact = 1.0;
    for (int k = 0; k < c; ++k) {
        if (k > 0) fact *= k;
        sum += std::pow(a, k) / fact;
    }
    fact *= c; // c!
    double last_term = std::pow(a, c) / fact / (1.0 - rho);
    sum += last_term;
    double P0 = 1.0 / sum;

    // Erlang-C: probability an arriving job must wait.
    r.erlang_c = last_term * P0;

    // Mean waiting time in queue: Wq = C(c,a) / (c*mu - lambda)
    double Wq_ms    = r.erlang_c / (static_cast<double>(c) * mu_ms - lambda_ms);
    r.avg_wait_ms   = Wq_ms;
    r.avg_queue_len = lambda_ms * Wq_ms; // Little's law: Lq = λ·Wq
    r.avg_sojourn_ms= Wq_ms + 1.0 / mu_ms;
    r.utilization   = rho;
    r.valid         = true;
    return r;
}

// ── M/G/1 (Pollaczek-Khinchine) ──────────────────────────────────────────────

MG1Result computeMG1(double lambda_ms, double E_S_ms, double E_S2_ms2) {
    MG1Result r;
    if (lambda_ms <= 0.0 || E_S_ms <= 0.0) return r;

    double rho = lambda_ms * E_S_ms;
    if (rho >= 1.0) return r; // unstable

    // P-K mean waiting time: Wq = lambda * E[S²] / (2*(1-rho))
    double Wq_ms    = (lambda_ms * E_S2_ms2) / (2.0 * (1.0 - rho));
    r.avg_wait_ms   = Wq_ms;
    r.avg_queue_len = lambda_ms * Wq_ms;
    r.avg_sojourn_ms= Wq_ms + E_S_ms;
    r.valid         = true;
    return r;
}

// ── Comparison table ──────────────────────────────────────────────────────────

void printQueueingModelComparison(const MMcResult& mmc,
                                  const MG1Result& mg1,
                                  double sim_avg_wait_ms,
                                  double sim_avg_queue_len,
                                  double sim_avg_sojourn_ms) {
    constexpr int W1 = 28, W2 = 16;

    auto fmt = [](double v, bool valid) -> std::string {
        if (!valid) return "  (unstable)";
        std::ostringstream o;
        o << std::fixed << std::setprecision(2) << v;
        return o.str();
    };

    auto printRow = [&](const std::string& name, double sim_val,
                        double mmc_val, bool mmc_ok,
                        double mg1_val, bool mg1_ok) {
        std::cout << std::left  << std::setw(W1) << name
                  << std::right << std::setw(W2) << std::fixed << std::setprecision(2) << sim_val
                  << std::setw(W2) << fmt(mmc_val, mmc_ok)
                  << std::setw(W2) << fmt(mg1_val, mg1_ok)
                  << "\n";
    };

    std::cout << "\n--- Formal Queuing Model Comparison ---\n";
    std::cout << std::left  << std::setw(W1) << "Metric"
              << std::right << std::setw(W2) << "Simulated"
              << std::setw(W2) << "M/M/c"
              << std::setw(W2) << "M/G/1"
              << "\n"
              << std::string(W1 + W2 * 3, '-') << "\n";

    printRow("Avg Wait (ms)",
             sim_avg_wait_ms,
             mmc.avg_wait_ms,   mmc.valid,
             mg1.avg_wait_ms,   mg1.valid);

    printRow("Avg Queue Length",
             sim_avg_queue_len,
             mmc.avg_queue_len, mmc.valid,
             mg1.avg_queue_len, mg1.valid);

    printRow("Avg Sojourn (ms)",
             sim_avg_sojourn_ms,
             mmc.avg_sojourn_ms, mmc.valid,
             mg1.avg_sojourn_ms, mg1.valid);

    if (mmc.valid) {
        std::cout << std::left  << std::setw(W1) << "Erlang-C P(wait)"
                  << std::right << std::setw(W2) << "-"
                  << std::setw(W2) << std::fixed << std::setprecision(3) << mmc.erlang_c
                  << std::setw(W2) << "-"
                  << "\n";
        std::cout << std::left  << std::setw(W1) << "Utilisation"
                  << std::right << std::setw(W2) << "-"
                  << std::setw(W2) << std::fixed << std::setprecision(3) << mmc.utilization
                  << std::setw(W2) << "-"
                  << "\n";
    }
    std::cout << "\n";
}
