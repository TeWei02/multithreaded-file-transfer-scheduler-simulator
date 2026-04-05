#include "simulation.h"
#include "server.h"
#include "qos.h"
#include "queueing_model.h"
#include "web_dashboard.h"
#include "trace_replay.h"
#include "socket_engine.h"
#include "congestion.h"
#include "packet_scheduler.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <memory>

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string fmt1(double v) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(1) << v;
    return o.str();
}

static std::string fmt3(double v) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(3) << v;
    return o.str();
}

// ── Simulation ────────────────────────────────────────────────────────────────

Simulation::Simulation(SimConfig cfg) : cfg_(std::move(cfg)) {
    // Truncate CSV files at startup.
    {
        std::ofstream f(results_csv_path_, std::ios::trunc);
        f << "policy,id,user_id,size_mb,direction,priority,"
             "arrival_ms,start_ms,completion_ms,wait_ms,tat_ms\n";
    }
    {
        std::ofstream f(comparison_csv_path_, std::ios::trunc);
        (void)f; // header written by first writePolicyComparisonCsv call
    }
}

void Simulation::run() {
    printHeader();

    // Extension 6: Start web dashboard if requested.
    std::unique_ptr<WebDashboard> dash;
    if (cfg_.web_dashboard) {
        dash = std::make_unique<WebDashboard>(cfg_.web_port);
        dash->start();
    }

    static const SchedulingPolicy ALL_POLICIES[] = {
        SchedulingPolicy::FCFS,
        SchedulingPolicy::SJF,
        SchedulingPolicy::PRIORITY,
        SchedulingPolicy::ROUND_ROBIN,
        SchedulingPolicy::WFQ
    };

    for (auto p : ALL_POLICIES) {
        std::string name = policyName(p);
        if (cfg_.policy_filter != "all") {
            std::string wanted = cfg_.policy_filter;
            if (wanted == "rr") wanted = "roundrobin";
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower != wanted) continue;
        }
        runPolicy(p, dash.get());
    }

    printComparisonTable();

    if (dash) dash->stop();
}

std::vector<TransferJob> Simulation::generateJobs() const {
    // Extension 5: Load from trace file if specified.
    if (!cfg_.trace_file.empty()) {
        auto jobs = loadTraceCSV(cfg_.trace_file);
        return jobs;
    }
#ifdef HAVE_LIBPCAP
    if (!cfg_.pcap_file.empty()) {
        auto jobs = loadTracePCAP(cfg_.pcap_file);
        return jobs;
    }
#endif

    std::mt19937 rng(static_cast<unsigned>(cfg_.seed));
    std::uniform_real_distribution<double> size_dist(5.0, 100.0);
    std::uniform_int_distribution<int>     user_dist(1, 10);
    std::uniform_int_distribution<int>     pri_dist(1, 10);
    std::uniform_int_distribution<int>     dir_dist(0, 1);
    std::uniform_real_distribution<double> gap_dist(0.0, 200.0);

    std::vector<TransferJob> jobs;
    jobs.reserve(cfg_.num_jobs);
    double arrival = 0.0;
    for (int i = 0; i < cfg_.num_jobs; ++i) {
        TransferJob j;
        j.id                = i + 1;
        j.user_id           = user_dist(rng);
        j.size_mb           = std::round(size_dist(rng) * 10.0) / 10.0;
        j.direction         = dir_dist(rng) == 0
                              ? TransferDirection::UPLOAD
                              : TransferDirection::DOWNLOAD;
        j.priority          = pri_dist(rng);
        j.arrival_ms        = arrival;
        j.start_ms          = arrival;
        j.first_start_ms    = 0.0;
        j.remaining_size_mb = j.size_mb;
        jobs.push_back(j);
        arrival += gap_dist(rng);
    }
    return jobs;
}

void Simulation::runPolicy(SchedulingPolicy policy, WebDashboard* dash) {
    std::string name = policyName(policy);

    // Extension 4: If QoS is enabled, override priority using PHB mapping.
    // (We run with the Priority policy so PHB priorities take effect.)

    double quantum_mb = (cfg_.rr_quantum_ms / 1000.0) * cfg_.bandwidth;
    Scheduler    sched(policy, quantum_mb, cfg_.bandwidth);
    MetricsCollector metrics;
    Server       server(cfg_.num_workers, cfg_.bandwidth,
                        cfg_.speed_factor, sched, metrics);

    auto jobs = generateJobs();

    // Extension 4: Remap job priorities to DiffServ PHB priorities.
    if (cfg_.qos) {
        for (auto& j : jobs)
            j.priority = phbPriority(classifyUser(j.user_id));
    }

    // Extension 1: Socket engine setup.
    int loopback_port = 0;
    int loopback_fd   = -1;
    if (cfg_.engine == "socket") {
        loopback_port = startLoopbackServer();
        loopback_fd   = loopback_port; // reuse port as handle identifier
        if (loopback_port == 0)
            std::cerr << "[socket] Failed to start loopback server; "
                         "falling back to sim mode.\n";
    }

    // Extension 2: AIMD congestion controller.
    std::unique_ptr<CongestionController> congestion;
    if (cfg_.congestion)
        congestion = std::make_unique<CongestionController>(32.0, 0.80);

    // Extension 3: Packet-level scheduling info output.
    std::unique_ptr<DRRScheduler> drr_sched;
    std::unique_ptr<HTBScheduler> htb_sched;
    if (cfg_.packet_level) {
        if (cfg_.pkt_scheduler == "htb")
            htb_sched = std::make_unique<HTBScheduler>();
        else
            drr_sched = std::make_unique<DRRScheduler>(cfg_.packet_size_mb);

        // Enqueue all jobs as packets into the packet scheduler for preview.
        int seq_counter = 0;
        for (const auto& j : jobs) {
            double remaining = j.size_mb;
            int seq = 0;
            while (remaining > 1e-9) {
                Packet pkt;
                pkt.job_id        = j.id;
                pkt.flow_id       = j.user_id;
                pkt.size_mb       = std::min(remaining, cfg_.packet_size_mb);
                pkt.seq           = seq++;
                pkt.last          = (remaining - pkt.size_mb < 1e-9);
                pkt.job_arrival_ms= j.arrival_ms;
                pkt.job_start_ms  = j.start_ms;
                remaining        -= pkt.size_mb;
                ++seq_counter;
                if (drr_sched) drr_sched->enqueue(pkt);
                else if (htb_sched) htb_sched->enqueue(pkt);
            }
        }
        std::cout << "[packet-level/" << cfg_.pkt_scheduler
                  << "] Enqueued " << seq_counter << " packets from "
                  << jobs.size() << " flows.\n";
    }

    // Record wall-clock start and set it on the server.
    auto wall_start = std::chrono::steady_clock::now();
    server.setSimStart(wall_start);

    // Start workers.
    server.start();

    // Producer: enqueue jobs with simulated inter-arrival delays.
    for (size_t i = 0; i < jobs.size(); ++i) {
        if (i > 0 && jobs[i].arrival_ms > 0.0) {
            double gap_ms  = jobs[i].arrival_ms - jobs[i - 1].arrival_ms;
            double wall_us = (gap_ms / static_cast<double>(cfg_.speed_factor)) * 1000.0;
            std::this_thread::sleep_for(
                std::chrono::microseconds(static_cast<long long>(wall_us)));
        }
        sched.addJob(jobs[i]);
    }

    sched.shutdown();
    server.join();

    auto wall_end = std::chrono::steady_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(wall_end - wall_start).count();
    double sim_ms  = wall_ms * static_cast<double>(cfg_.speed_factor);

    // Extension 2: Report congestion info if enabled.
    if (congestion) {
        double util = server.totalServiceTimeMs() /
                      (cfg_.num_workers * std::max(sim_ms, 1.0));
        std::cout << "[congestion/AIMD] Server utilisation: "
                  << fmt1(util * 100.0) << "%"
                  << (congestion->isCongested(util) ? "  ← congestion detected" : "")
                  << "\n";
    }

    SimStats stats = metrics.computeStats(name, sim_ms,
                                          server.totalServiceTimeMs(),
                                          cfg_.num_workers);

    // Collect completed jobs for printing.
    auto completed = metrics.completedJobs();
    std::sort(completed.begin(), completed.end(),
              [](const TransferJob& a, const TransferJob& b){ return a.id < b.id; });

    // Extension 6: Push each job to the web dashboard.
    if (dash) {
        for (const auto& j : completed)
            dash->push(j, name);
    }

    printJobTable(completed, name);
    printStats(stats);

    // Extension 4: DiffServ class statistics.
    if (cfg_.qos)
        printQoSStats(computeQoSStats(completed, sim_ms));

    // Extension 7: Formal queuing model comparison.
    if (cfg_.show_queueing_models && stats.completed > 0 && sim_ms > 0.0) {
        double lambda    = stats.completed / (sim_ms / 1000.0); // jobs/s
        double lambda_ms = lambda / 1000.0;                     // jobs/ms
        double E_S_ms    = stats.avg_tat_ms - stats.avg_wait_ms;
        if (E_S_ms <= 0.0) E_S_ms = 1.0;
        double mu_ms     = 1.0 / E_S_ms;
        // For M/G/1, assume exponential service: E[S²] = 2/μ² = 2·E[S]²
        double E_S2_ms2  = 2.0 * E_S_ms * E_S_ms;
        double avg_queue = lambda_ms * stats.avg_wait_ms; // Little's law

        MMcResult mmc = computeMMc(lambda_ms, mu_ms, cfg_.num_workers);
        MG1Result mg1 = computeMG1(lambda_ms, E_S_ms, E_S2_ms2);
        printQueueingModelComparison(mmc, mg1,
                                     stats.avg_wait_ms,
                                     avg_queue,
                                     stats.avg_tat_ms);
    }

    // CSV output.
    metrics.writeResultsCsv(results_csv_path_, name);
    metrics.writePolicyComparisonCsv(comparison_csv_path_, stats, first_policy_run_);
    first_policy_run_ = false;

    all_stats_.push_back(stats);

    // Clean up socket engine.
    if (loopback_fd > 0) stopLoopbackServer(loopback_fd);
}

void Simulation::printHeader() const {
    std::cout << "=======================================================\n"
              << "  Multithreaded File Transfer Scheduler Simulator\n"
              << "=======================================================\n"
              << "Config: jobs=" << cfg_.num_jobs
              << "  workers=" << cfg_.num_workers
              << "  bandwidth=" << cfg_.bandwidth << " MB/s"
              << "  seed=" << cfg_.seed << "\n\n";
}

void Simulation::printJobTable(const std::vector<TransferJob>& jobs,
                               const std::string& policy,
                               int max_rows) const {
    std::cout << "--- Job Results (policy: " << policy
              << ", first " << std::min((int)jobs.size(), max_rows)
              << " shown) ---\n";
    std::cout << std::setw(4)  << "ID"
              << std::setw(6)  << "User"
              << std::setw(10) << "Size(MB)"
              << std::setw(5)  << "Dir"
              << std::setw(5)  << "Pri"
              << std::setw(10) << "Arrival"
              << std::setw(10) << "Start"
              << std::setw(10) << "Complete"
              << std::setw(9)  << "Wait"
              << std::setw(9)  << "TAT"
              << "\n";
    std::cout << std::string(77, '-') << "\n";

    int shown = 0;
    for (const auto& j : jobs) {
        if (shown >= max_rows) break;
        std::cout << std::fixed << std::setprecision(1)
                  << std::setw(4)  << j.id
                  << std::setw(6)  << j.user_id
                  << std::setw(10) << j.size_mb
                  << std::setw(5)  << j.directionStr()
                  << std::setw(5)  << j.priority
                  << std::setw(10) << j.arrival_ms
                  << std::setw(10) << j.start_ms
                  << std::setw(10) << j.completion_ms
                  << std::setw(9)  << (j.start_ms - j.arrival_ms)
                  << std::setw(9)  << (j.completion_ms - j.arrival_ms)
                  << "\n";
        ++shown;
    }
    std::cout << "\n";
}

void Simulation::printStats(const SimStats& s) const {
    std::cout << "--- Overall Statistics [" << s.policy_name << "] ---\n"
              << "  Completed jobs    : " << s.completed << "\n"
              << "  Avg waiting time  : " << fmt1(s.avg_wait_ms) << " ms\n"
              << "  Avg turnaround    : " << fmt1(s.avg_tat_ms)  << " ms\n"
              << "  Throughput        : " << fmt3(s.throughput)  << " jobs/s\n"
              << "  Server utilization: " << fmt1(s.utilization * 100.0) << " %\n"
              << "  Jain fairness idx : " << fmt3(s.jains_fairness) << "\n\n";
}

void Simulation::printComparisonTable() const {
    std::cout << "--- Policy Comparison ---\n";
    std::cout << std::left  << std::setw(14) << "Policy"
              << std::right << std::setw(13) << "AvgWait(ms)"
              << std::setw(12) << "AvgTAT(ms)"
              << std::setw(12) << "Throughput"
              << std::setw(13) << "Utilization"
              << std::setw(10) << "Fairness"
              << "\n";
    std::cout << std::string(74, '-') << "\n";

    for (const auto& s : all_stats_) {
        std::ostringstream util_str;
        util_str << std::fixed << std::setprecision(1) << (s.utilization * 100.0) << "%";
        std::cout << std::fixed << std::setprecision(1)
                  << std::left  << std::setw(14) << s.policy_name
                  << std::right << std::setw(13) << s.avg_wait_ms
                  << std::setw(12) << s.avg_tat_ms
                  << std::setw(12) << std::setprecision(3) << s.throughput
                  << std::setw(13) << util_str.str()
                  << std::setw(10) << std::setprecision(3) << s.jains_fairness
                  << "\n";
    }
    std::cout << "=======================================================\n";
}
