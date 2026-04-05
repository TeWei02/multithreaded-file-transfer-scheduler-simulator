#include "scheduler.h"
#include <stdexcept>
#include <algorithm>

constexpr double Scheduler::WFQ_WEIGHTS[3];

std::string policyName(SchedulingPolicy p) {
    switch (p) {
        case SchedulingPolicy::FCFS:        return "FCFS";
        case SchedulingPolicy::SJF:         return "SJF";
        case SchedulingPolicy::PRIORITY:    return "Priority";
        case SchedulingPolicy::ROUND_ROBIN: return "RoundRobin";
        case SchedulingPolicy::WFQ:         return "WFQ";
    }
    return "Unknown";
}

SchedulingPolicy parsePolicy(const std::string& s) {
    if (s == "fcfs")     return SchedulingPolicy::FCFS;
    if (s == "sjf")      return SchedulingPolicy::SJF;
    if (s == "priority") return SchedulingPolicy::PRIORITY;
    if (s == "rr")       return SchedulingPolicy::ROUND_ROBIN;
    if (s == "wfq")      return SchedulingPolicy::WFQ;
    throw std::invalid_argument("Unknown policy: " + s);
}

Scheduler::Scheduler(SchedulingPolicy policy, double rr_quantum_mb, double bandwidth_mbps)
    : policy_(policy), rr_quantum_mb_(rr_quantum_mb), bandwidth_mbps_(bandwidth_mbps) {}

void Scheduler::addJob(TransferJob job) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        switch (policy_) {
            case SchedulingPolicy::FCFS:
            case SchedulingPolicy::ROUND_ROBIN:
                fifo_.push_back(job);
                break;
            case SchedulingPolicy::SJF:
                sjf_pq_.push(job);
                break;
            case SchedulingPolicy::PRIORITY:
                pri_pq_.push(job);
                break;
            case SchedulingPolicy::WFQ: {
                int group = job.user_id % 3;
                double weight = WFQ_WEIGHTS[group];
                // Virtual finish time = max(virtual_time, job.arrival_ms) + size / weight
                double vstart = std::max(virtual_time_, job.arrival_ms);
                job.virtual_finish_time = vstart + job.size_mb / weight;
                wfq_pq_.push(job);
                break;
            }
        }
    }
    cv_.notify_one();
}

void Scheduler::requeue(TransferJob job) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        fifo_.push_back(job);
    }
    cv_.notify_one();
}

std::optional<TransferJob> Scheduler::getNextJob() {
    std::unique_lock<std::mutex> lock(mutex_);
    auto has_job = [this]() -> bool {
        switch (policy_) {
            case SchedulingPolicy::FCFS:
            case SchedulingPolicy::ROUND_ROBIN:
                return !fifo_.empty();
            case SchedulingPolicy::SJF:
                return !sjf_pq_.empty();
            case SchedulingPolicy::PRIORITY:
                return !pri_pq_.empty();
            case SchedulingPolicy::WFQ:
                return !wfq_pq_.empty();
        }
        return false;
    };

    cv_.wait(lock, [&]{ return has_job() || shutdown_; });
    if (!has_job()) return std::nullopt;

    TransferJob job;
    switch (policy_) {
        case SchedulingPolicy::FCFS:
        case SchedulingPolicy::ROUND_ROBIN:
            job = fifo_.front();
            fifo_.pop_front();
            break;
        case SchedulingPolicy::SJF:
            job = sjf_pq_.top();
            sjf_pq_.pop();
            break;
        case SchedulingPolicy::PRIORITY:
            job = pri_pq_.top();
            pri_pq_.pop();
            break;
        case SchedulingPolicy::WFQ:
            job = wfq_pq_.top();
            wfq_pq_.pop();
            virtual_time_ = job.virtual_finish_time;
            break;
    }
    return job;
}

void Scheduler::shutdown() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    cv_.notify_all();
}

size_t Scheduler::pendingCount() const {
    std::unique_lock<std::mutex> lock(mutex_);
    switch (policy_) {
        case SchedulingPolicy::FCFS:
        case SchedulingPolicy::ROUND_ROBIN: return fifo_.size();
        case SchedulingPolicy::SJF:         return sjf_pq_.size();
        case SchedulingPolicy::PRIORITY:    return pri_pq_.size();
        case SchedulingPolicy::WFQ:         return wfq_pq_.size();
    }
    return 0;
}
