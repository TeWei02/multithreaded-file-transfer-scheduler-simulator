#include "scheduler.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace simulator {

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return s;
}

double SafeBandwidth(const Request& request) {
    return std::max(0.001, request.estimated_bandwidth_mb_per_sec);
}

}  // namespace

void FifoScheduler::push(int request_index, const std::vector<Request>&, double) {
    queue_.push_back(request_index);
}

std::optional<int> FifoScheduler::pop(const std::vector<Request>&, double) {
    if (queue_.empty()) {
        return std::nullopt;
    }
    int id = queue_.front();
    queue_.pop_front();
    return id;
}

bool FifoScheduler::empty() const {
    return queue_.empty();
}

std::string FifoScheduler::name() const {
    return "fifo";
}

SchedulerType FifoScheduler::type() const {
    return SchedulerType::kFifo;
}

void SjfScheduler::push(int request_index, const std::vector<Request>& requests, double) {
    const Request& request = requests[request_index];
    double estimated_time = request.remaining_size_mb / SafeBandwidth(request);
    heap_.push(Node{estimated_time, seq_++, request_index});
}

std::optional<int> SjfScheduler::pop(const std::vector<Request>&, double) {
    if (heap_.empty()) {
        return std::nullopt;
    }
    Node node = heap_.top();
    heap_.pop();
    return node.request_index;
}

bool SjfScheduler::empty() const {
    return heap_.empty();
}

std::string SjfScheduler::name() const {
    return "sjf";
}

SchedulerType SjfScheduler::type() const {
    return SchedulerType::kSjf;
}

void PriorityScheduler::push(int request_index, const std::vector<Request>& requests, double) {
    const Request& request = requests[request_index];
    heap_.push(Node{request.priority, seq_++, request_index});
}

std::optional<int> PriorityScheduler::pop(const std::vector<Request>&, double) {
    if (heap_.empty()) {
        return std::nullopt;
    }
    Node node = heap_.top();
    heap_.pop();
    return node.request_index;
}

bool PriorityScheduler::empty() const {
    return heap_.empty();
}

std::string PriorityScheduler::name() const {
    return "priority";
}

SchedulerType PriorityScheduler::type() const {
    return SchedulerType::kPriority;
}

RoundRobinScheduler::RoundRobinScheduler(double quantum_sec)
    : quantum_sec_(std::max(0.001, quantum_sec)) {}

void RoundRobinScheduler::push(int request_index, const std::vector<Request>&, double) {
    queue_.push_back(request_index);
}

std::optional<int> RoundRobinScheduler::pop(const std::vector<Request>&, double) {
    if (queue_.empty()) {
        return std::nullopt;
    }
    int id = queue_.front();
    queue_.pop_front();
    return id;
}

bool RoundRobinScheduler::empty() const {
    return queue_.empty();
}

std::string RoundRobinScheduler::name() const {
    return "rr";
}

SchedulerType RoundRobinScheduler::type() const {
    return SchedulerType::kRoundRobin;
}

double RoundRobinScheduler::quantum_sec() const {
    return quantum_sec_;
}

double WfqLikeScheduler::WeightForPriority(int priority) {
    int clamped = std::min(10, std::max(1, priority));
    return static_cast<double>(11 - clamped);
}

void WfqLikeScheduler::push(int request_index, const std::vector<Request>& requests, double) {
    const Request& request = requests[request_index];
    double weight = WeightForPriority(request.priority);
    double service = request.remaining_size_mb / SafeBandwidth(request);
    double client_last = 0.0;

    auto it = client_finish_tag_.find(request.client_id);
    if (it != client_finish_tag_.end()) {
        client_last = it->second;
    }

    double start_tag = std::max(virtual_time_, client_last);
    double finish_tag = start_tag + (service / weight);

    client_finish_tag_[request.client_id] = finish_tag;
    heap_.push(Node{finish_tag, seq_++, request_index});
}

std::optional<int> WfqLikeScheduler::pop(const std::vector<Request>&, double) {
    if (heap_.empty()) {
        return std::nullopt;
    }
    Node node = heap_.top();
    heap_.pop();
    virtual_time_ = std::max(virtual_time_, node.finish_tag);
    return node.request_index;
}

bool WfqLikeScheduler::empty() const {
    return heap_.empty();
}

std::string WfqLikeScheduler::name() const {
    return "wfq";
}

SchedulerType WfqLikeScheduler::type() const {
    return SchedulerType::kWfq;
}

std::unique_ptr<IScheduler> CreateScheduler(SchedulerType type, double quantum_sec) {
    switch (type) {
        case SchedulerType::kFifo:
            return std::make_unique<FifoScheduler>();
        case SchedulerType::kSjf:
            return std::make_unique<SjfScheduler>();
        case SchedulerType::kPriority:
            return std::make_unique<PriorityScheduler>();
        case SchedulerType::kRoundRobin:
            return std::make_unique<RoundRobinScheduler>(quantum_sec);
        case SchedulerType::kWfq:
            return std::make_unique<WfqLikeScheduler>();
    }
    throw std::runtime_error("Unknown scheduler type");
}

std::string SchedulerTypeToString(SchedulerType type) {
    switch (type) {
        case SchedulerType::kFifo:
            return "fifo";
        case SchedulerType::kSjf:
            return "sjf";
        case SchedulerType::kPriority:
            return "priority";
        case SchedulerType::kRoundRobin:
            return "rr";
        case SchedulerType::kWfq:
            return "wfq";
    }
    return "unknown";
}

std::optional<SchedulerType> ParseSchedulerType(const std::string& raw) {
    std::string v = ToLower(raw);
    if (v == "fifo") {
        return SchedulerType::kFifo;
    }
    if (v == "sjf") {
        return SchedulerType::kSjf;
    }
    if (v == "priority") {
        return SchedulerType::kPriority;
    }
    if (v == "rr" || v == "round_robin" || v == "round-robin") {
        return SchedulerType::kRoundRobin;
    }
    if (v == "wfq" || v == "wfq-like") {
        return SchedulerType::kWfq;
    }
    return std::nullopt;
}

}  // namespace simulator
