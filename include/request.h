#ifndef REQUEST_H
#define REQUEST_H

#include <string>

namespace simulator {

struct Request {
    int request_id = 0;
    int client_id = 0;
    double arrival_time = 0.0;
    double file_size_mb = 0.0;
    double estimated_bandwidth_mb_per_sec = 1.0;
    int priority = 5;

    double remaining_size_mb = 0.0;
    double first_start_time = -1.0;
    double finish_time = -1.0;
    double total_processing_time = 0.0;
    int assigned_thread = -1;
    int dispatch_count = 0;
};

struct RequestResult {
    int request_id = 0;
    int client_id = 0;
    double arrival_time = 0.0;
    double start_time = 0.0;
    double finish_time = 0.0;
    double waiting_time = 0.0;
    double turnaround_time = 0.0;
    double response_time = 0.0;
    int assigned_thread = -1;
    std::string scheduler_name;
};

struct MetricsSummary {
    std::string scheduler;
    double avg_waiting_time = 0.0;
    double avg_turnaround_time = 0.0;
    double avg_response_time = 0.0;
    double throughput = 0.0;
    double fairness_index = 0.0;
    double total_completion_time = 0.0;
};

}  // namespace simulator

#endif
