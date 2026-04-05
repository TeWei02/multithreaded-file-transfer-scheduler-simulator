#include "simulation_engine.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>

#include "csv_utils.h"
#include "metrics.h"
#include "worker_manager.h"

namespace simulator {

namespace {

struct CompletionEvent {
    double finish_time = 0.0;
    int worker_id = -1;
    int request_index = -1;
    double processed_mb = 0.0;
    double slice_time = 0.0;
};

struct CompletionCompare {
    bool operator()(const CompletionEvent& a, const CompletionEvent& b) const {
        return a.finish_time > b.finish_time;
    }
};

double SafeBandwidth(const Request& req) {
    return std::max(0.001, req.estimated_bandwidth_mb_per_sec);
}

std::vector<SchedulerType> AllSchedulers() {
    return {
        SchedulerType::kFifo,
        SchedulerType::kSjf,
        SchedulerType::kPriority,
        SchedulerType::kRoundRobin,
        SchedulerType::kWfq,
    };
}

}  // namespace

SimulationEngine::SimulationEngine(SimulationConfig config) : config_(std::move(config)) {}

int SimulationEngine::GenerateSample(const std::string& output_path, std::size_t n, unsigned int seed) {
    try {
        GenerateSampleRequestsCsv(output_path, n, seed);
        std::cout << "Sample dataset generated: " << output_path << " (n=" << n << ", seed=" << seed << ")\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Failed to generate sample dataset: " << ex.what() << '\n';
        return 1;
    }
}

int SimulationEngine::Run() {
    if (config_.threads <= 0) {
        std::cerr << "--threads must be >= 1\n";
        return 1;
    }

    std::vector<Request> requests;
    try {
        requests = ReadRequestsCsv(config_.input_path);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    if (requests.empty()) {
        std::cerr << "Input file contains no requests\n";
        return 1;
    }

    std::filesystem::create_directories(config_.output_dir);

    if (config_.benchmark) {
        std::vector<MetricsSummary> summaries;
        for (SchedulerType type : AllSchedulers()) {
            RunOutput out = RunOneScheduler(type, requests);
            const std::string path = config_.output_dir + "/result_" + SchedulerTypeToString(type) + ".csv";
            WriteRequestResultsCsv(path, out.rows);
            summaries.push_back(out.summary);
        }

        const std::string summary_path = config_.output_dir + "/summary.csv";
        WriteSummaryCsv(summary_path, summaries);
        PrintBenchmarkSummary(summaries);
        std::cout << "\nBenchmark summary written to: " << summary_path << '\n';
        return 0;
    }

    RunOutput out = RunOneScheduler(config_.scheduler, requests);
    const std::string result_path =
        config_.output_dir + "/result_" + SchedulerTypeToString(config_.scheduler) + ".csv";
    WriteRequestResultsCsv(result_path, out.rows);

    PrintConsoleSummary(out.summary);
    std::cout << "Request-level result written to: " << result_path << '\n';
    return 0;
}

SimulationEngine::RunOutput SimulationEngine::RunOneScheduler(
    SchedulerType type, const std::vector<Request>& original_requests) const {
    std::vector<Request> requests = original_requests;
    for (auto& req : requests) {
        req.remaining_size_mb = req.file_size_mb;
        req.first_start_time = -1.0;
        req.finish_time = -1.0;
        req.total_processing_time = 0.0;
        req.assigned_thread = -1;
        req.dispatch_count = 0;
    }

    auto scheduler = CreateScheduler(type, config_.quantum_sec);
    WorkerManager workers(static_cast<std::size_t>(config_.threads));

    struct WorkerSlot {
        bool busy = false;
        int request_index = -1;
    };

    std::vector<WorkerSlot> slots(static_cast<std::size_t>(config_.threads));
    std::priority_queue<CompletionEvent, std::vector<CompletionEvent>, CompletionCompare> events;

    std::size_t arrived = 0;
    std::size_t completed = 0;
    const std::size_t total = requests.size();
    double sim_time = 0.0;

    auto enqueue_arrivals = [&](double now) {
        while (arrived < total && requests[arrived].arrival_time <= now + 1e-9) {
            scheduler->push(static_cast<int>(arrived), requests, now);
            ++arrived;
        }
    };

    while (completed < total) {
        enqueue_arrivals(sim_time);

        for (std::size_t worker_id = 0; worker_id < slots.size(); ++worker_id) {
            if (slots[worker_id].busy) {
                continue;
            }
            if (scheduler->empty()) {
                break;
            }

            const std::optional<int> request_index_opt = scheduler->pop(requests, sim_time);
            if (!request_index_opt.has_value()) {
                break;
            }

            const int request_index = *request_index_opt;
            Request& req = requests[request_index];
            if (req.first_start_time < 0.0) {
                req.first_start_time = sim_time;
            }

            req.assigned_thread = static_cast<int>(worker_id);
            req.dispatch_count += 1;

            const double bandwidth = SafeBandwidth(req);
            const double full_time = req.remaining_size_mb / bandwidth;

            double transfer_time = full_time;
            double processed_mb = req.remaining_size_mb;

            if (type == SchedulerType::kRoundRobin) {
                transfer_time = std::min(full_time, std::max(0.001, config_.quantum_sec));
                processed_mb = std::min(req.remaining_size_mb, transfer_time * bandwidth);
            }

            const double slice_time = transfer_time + config_.connection_overhead_sec;
            const double finish = sim_time + slice_time;

            slots[worker_id].busy = true;
            slots[worker_id].request_index = request_index;

            workers.SubmitSlice(static_cast<int>(worker_id), req.request_id, sim_time, finish);
            events.push(CompletionEvent{finish, static_cast<int>(worker_id), request_index, processed_mb, slice_time});
        }

        const double next_arrival =
            (arrived < total) ? requests[arrived].arrival_time : std::numeric_limits<double>::infinity();
        const double next_finish = events.empty() ? std::numeric_limits<double>::infinity() : events.top().finish_time;

        if (next_arrival == std::numeric_limits<double>::infinity() &&
            next_finish == std::numeric_limits<double>::infinity()) {
            break;
        }

        if (scheduler->empty() && next_finish == std::numeric_limits<double>::infinity() && next_arrival > sim_time) {
            sim_time = next_arrival;
            continue;
        }

        sim_time = std::min(next_arrival, next_finish);

        enqueue_arrivals(sim_time);

        while (!events.empty() && events.top().finish_time <= sim_time + 1e-9) {
            CompletionEvent ev = events.top();
            events.pop();

            slots[ev.worker_id].busy = false;
            slots[ev.worker_id].request_index = -1;

            Request& req = requests[ev.request_index];
            req.remaining_size_mb = std::max(0.0, req.remaining_size_mb - ev.processed_mb);
            req.total_processing_time += ev.slice_time;

            if (req.remaining_size_mb <= 1e-9) {
                req.remaining_size_mb = 0.0;
                req.finish_time = ev.finish_time;
                ++completed;
            } else {
                scheduler->push(ev.request_index, requests, sim_time);
            }
        }
    }

    workers.Stop();

    double total_completion_time = 0.0;
    for (const auto& req : requests) {
        total_completion_time = std::max(total_completion_time, req.finish_time);
    }

    const std::string scheduler_name = SchedulerTypeToString(type);
    MetricsSummary summary = MetricsCollector::BuildSummary(requests, scheduler_name, total_completion_time);
    std::vector<RequestResult> rows = MetricsCollector::BuildRequestResults(requests, scheduler_name);

    return RunOutput{summary, rows};
}

void SimulationEngine::PrintConsoleSummary(const MetricsSummary& summary) const {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Scheduler: " << summary.scheduler << '\n';
    std::cout << "Average waiting time   : " << summary.avg_waiting_time << " sec\n";
    std::cout << "Average turnaround time: " << summary.avg_turnaround_time << " sec\n";
    std::cout << "Average response time  : " << summary.avg_response_time << " sec\n";
    std::cout << "Throughput             : " << summary.throughput << " req/sec\n";
    std::cout << "Fairness index         : " << summary.fairness_index << '\n';
    std::cout << "Total completion time  : " << summary.total_completion_time << " sec\n";
}

void SimulationEngine::PrintBenchmarkSummary(const std::vector<MetricsSummary>& summaries) const {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Scheduler Benchmark Summary\n";
    std::cout << "--------------------------------------------------------------------------------------------\n";
    std::cout << "scheduler   avg_wait(s)   avg_turnaround(s)   avg_response(s)   throughput   fairness\n";
    std::cout << "--------------------------------------------------------------------------------------------\n";

    for (const auto& s : summaries) {
        std::cout << std::left << std::setw(10) << s.scheduler << " " << std::right << std::setw(11)
                  << s.avg_waiting_time << " " << std::setw(18) << s.avg_turnaround_time << " "
                  << std::setw(16) << s.avg_response_time << " " << std::setw(11) << s.throughput << " "
                  << std::setw(9) << s.fairness_index << '\n';
    }
}

}  // namespace simulator
