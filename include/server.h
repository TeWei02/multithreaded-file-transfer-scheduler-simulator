#pragma once
#include "request.h"
#include "scheduler.h"
#include "metrics.h"
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

// Server manages a pool of worker threads.
// Each worker repeatedly fetches a job from the Scheduler,
// simulates the transfer, then records the result via MetricsCollector.
class Server {
public:
    Server(int num_workers,
           double bandwidth_mbps,
           int    speed_factor,
           Scheduler& scheduler,
           MetricsCollector& metrics);

    ~Server();

    // Record the simulation wall-clock start time (called just before
    // enqueuing the first job so workers can compute simulated time).
    void setSimStart(std::chrono::steady_clock::time_point t);

    // Start all worker threads.
    void start();

    // Join all worker threads (call after scheduler.shutdown()).
    void join();

    // Total simulated service time contributed by all workers (ms).
    double totalServiceTimeMs() const;

    // Current simulated time in ms (wall elapsed * speed_factor).
    double simNow() const;

private:
    void workerLoop();

    int               num_workers_;
    double            bandwidth_mbps_;
    int               speed_factor_;
    Scheduler&        scheduler_;
    MetricsCollector& metrics_;

    std::chrono::steady_clock::time_point sim_start_{};

    std::vector<std::thread> threads_;
    std::atomic<double>      total_service_ms_{0.0};
};

