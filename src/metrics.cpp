#include "metrics.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace simulator {

std::vector<RequestResult> MetricsCollector::BuildRequestResults(
    const std::vector<Request>& requests, const std::string& scheduler_name) {
    std::vector<RequestResult> rows;
    rows.reserve(requests.size());

    for (const auto& req : requests) {
        if (req.finish_time < 0.0 || req.first_start_time < 0.0) {
            continue;
        }

        const double turnaround = req.finish_time - req.arrival_time;
        const double response = req.first_start_time - req.arrival_time;
        const double waiting = std::max(0.0, turnaround - req.total_processing_time);

        rows.push_back(RequestResult{
            req.request_id,
            req.client_id,
            req.arrival_time,
            req.first_start_time,
            req.finish_time,
            waiting,
            turnaround,
            response,
            req.assigned_thread,
            scheduler_name,
        });
    }

    std::sort(rows.begin(), rows.end(), [](const RequestResult& a, const RequestResult& b) {
        return a.request_id < b.request_id;
    });

    return rows;
}

MetricsSummary MetricsCollector::BuildSummary(
    const std::vector<Request>& requests,
    const std::string& scheduler_name,
    double total_completion_time) {
    MetricsSummary summary;
    summary.scheduler = scheduler_name;
    summary.total_completion_time = total_completion_time;

    const std::vector<RequestResult> rows = BuildRequestResults(requests, scheduler_name);
    if (rows.empty() || total_completion_time <= 0.0) {
        return summary;
    }

    const double n = static_cast<double>(rows.size());
    for (const auto& row : rows) {
        summary.avg_waiting_time += row.waiting_time;
        summary.avg_turnaround_time += row.turnaround_time;
        summary.avg_response_time += row.response_time;
    }

    summary.avg_waiting_time /= n;
    summary.avg_turnaround_time /= n;
    summary.avg_response_time /= n;
    summary.throughput = n / total_completion_time;

    struct ClientStats {
        double total_size = 0.0;
        double first_arrival = -1.0;
        double last_finish = -1.0;
    };

    std::unordered_map<int, ClientStats> clients;
    for (const auto& req : requests) {
        if (req.finish_time < 0.0) {
            continue;
        }
        auto& c = clients[req.client_id];
        c.total_size += req.file_size_mb;
        if (c.first_arrival < 0.0 || req.arrival_time < c.first_arrival) {
            c.first_arrival = req.arrival_time;
        }
        c.last_finish = std::max(c.last_finish, req.finish_time);
    }

    std::vector<double> throughputs;
    throughputs.reserve(clients.size());
    for (const auto& kv : clients) {
        const auto& c = kv.second;
        const double span = std::max(1e-6, c.last_finish - c.first_arrival);
        throughputs.push_back(c.total_size / span);
    }

    if (!throughputs.empty()) {
        const double sum = std::accumulate(throughputs.begin(), throughputs.end(), 0.0);
        double sq_sum = 0.0;
        for (double x : throughputs) {
            sq_sum += x * x;
        }

        if (sq_sum > 0.0) {
            const double nn = static_cast<double>(throughputs.size());
            summary.fairness_index = (sum * sum) / (nn * sq_sum);
        }
    }

    return summary;
}

}  // namespace simulator
