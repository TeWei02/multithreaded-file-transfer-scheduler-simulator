#include "server.h"
#include "metrics.h"
#include <chrono>
#include <thread>
#include <cmath>

using Clock = std::chrono::steady_clock;

Server::Server(int num_workers, double bandwidth_mbps, int speed_factor,
               Scheduler& scheduler, MetricsCollector& metrics)
    : num_workers_(num_workers), bandwidth_mbps_(bandwidth_mbps),
      speed_factor_(speed_factor), scheduler_(scheduler), metrics_(metrics),
      sim_start_(Clock::now()) {}

Server::~Server() { join(); }

void Server::setSimStart(std::chrono::steady_clock::time_point t) {
    sim_start_ = t;
}

void Server::start() {
    threads_.reserve(num_workers_);
    for (int i = 0; i < num_workers_; ++i)
        threads_.emplace_back(&Server::workerLoop, this);
}

void Server::join() {
    for (auto& t : threads_)
        if (t.joinable()) t.join();
    threads_.clear();
}

double Server::totalServiceTimeMs() const {
    return total_service_ms_.load();
}

double Server::simNow() const {
    double wall_ms = std::chrono::duration<double, std::milli>(
                         Clock::now() - sim_start_).count();
    return wall_ms * static_cast<double>(speed_factor_);
}

void Server::workerLoop() {
    const bool is_rr = (scheduler_.policy() == SchedulingPolicy::ROUND_ROBIN);

    while (true) {
        auto maybe = scheduler_.getNextJob();
        if (!maybe) break;

        TransferJob job = std::move(*maybe);

        // Stamp the actual simulated start time for this service episode.
        double now_sim = simNow();
        job.start_ms = now_sim;

        // Record the first time this job is touched by a worker.
        if (job.first_start_ms <= 0.0)
            job.first_start_ms = now_sim;

        double work_mb = job.remaining_size_mb;

        // For Round Robin, cap this scheduling quantum.
        if (is_rr && work_mb > scheduler_.rrQuantumMb())
            work_mb = scheduler_.rrQuantumMb();

        double service_ms = (work_mb / bandwidth_mbps_) * 1000.0;
        double wall_us    = (service_ms / static_cast<double>(speed_factor_)) * 1000.0;

        std::this_thread::sleep_for(
            std::chrono::microseconds(static_cast<long long>(wall_us)));

        // Accumulate service time atomically.
        double old_val = total_service_ms_.load(std::memory_order_relaxed);
        while (!total_service_ms_.compare_exchange_weak(
                   old_val, old_val + service_ms,
                   std::memory_order_relaxed, std::memory_order_relaxed)) {}

        job.remaining_size_mb -= work_mb;

        if (is_rr && job.remaining_size_mb > 1e-9) {
            // Re-queue for the next quantum.
            scheduler_.requeue(job);
        } else {
            job.remaining_size_mb = 0.0;
            job.completion_ms = job.start_ms + service_ms;
            metrics_.recordCompletion(job);
        }
    }
}
