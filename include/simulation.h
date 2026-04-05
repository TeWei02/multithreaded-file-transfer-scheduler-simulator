#pragma once
#include "scheduler.h"
#include "metrics.h"
#include <vector>
#include <string>

// Forward declaration for extension 6.
class WebDashboard;

// Configuration passed from CLI to Simulation.
struct SimConfig {
    int    num_jobs     = 20;
    int    num_workers  = 4;
    double bandwidth    = 80.0;   // MB/s
    int    speed_factor = 100;    // simulated ms per wall ms
    double rr_quantum_ms= 50.0;   // Round Robin quantum (simulated ms)
    int    seed         = 42;
    std::string policy_filter = "all"; // "all" or specific policy name

    // Extension flags
    std::string engine       = "sim";    // "sim" | "socket"
    bool  congestion         = false;
    bool  packet_level       = false;
    std::string pkt_scheduler= "drr";   // "drr" | "htb"
    double packet_size_mb    = 1.0;
    bool  qos                = false;
    std::string trace_file;              // CSV trace file
    std::string pcap_file;               // PCAP file (requires libpcap)
    bool  web_dashboard      = false;
    int   web_port           = 8080;
    bool  show_queueing_models = true;
};

// Top-level orchestrator.
class Simulation {
public:
    explicit Simulation(SimConfig cfg);
    void run();

private:
    void runPolicy(SchedulingPolicy policy, WebDashboard* dash = nullptr);
    std::vector<TransferJob> generateJobs() const;
    void printHeader() const;
    void printJobTable(const std::vector<TransferJob>& jobs,
                       const std::string& policy,
                       int max_rows = 10) const;
    void printStats(const SimStats& s) const;
    void printComparisonTable() const;

    SimConfig cfg_;
    std::vector<SimStats> all_stats_;
    std::string results_csv_path_     = "output/results.csv";
    std::string comparison_csv_path_  = "output/policy_comparison.csv";
    bool first_policy_run_ = true;
};
