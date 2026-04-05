#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <deque>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "request.h"

namespace simulator {

enum class SchedulerType {
    kFifo,
    kSjf,
    kPriority,
    kRoundRobin,
    kWfq
};

class IScheduler {
   public:
    virtual ~IScheduler() = default;
    virtual void push(int request_index, const std::vector<Request>& requests, double now) = 0;
    virtual std::optional<int> pop(const std::vector<Request>& requests, double now) = 0;
    virtual bool empty() const = 0;
    virtual std::string name() const = 0;
    virtual SchedulerType type() const = 0;
};

class FifoScheduler : public IScheduler {
   public:
    void push(int request_index, const std::vector<Request>& requests, double now) override;
    std::optional<int> pop(const std::vector<Request>& requests, double now) override;
    bool empty() const override;
    std::string name() const override;
    SchedulerType type() const override;

   private:
    std::deque<int> queue_;
};

class SjfScheduler : public IScheduler {
   public:
    void push(int request_index, const std::vector<Request>& requests, double now) override;
    std::optional<int> pop(const std::vector<Request>& requests, double now) override;
    bool empty() const override;
    std::string name() const override;
    SchedulerType type() const override;

   private:
    struct Node {
        double estimated_time;
        long long seq;
        int request_index;
    };

    struct Compare {
        bool operator()(const Node& a, const Node& b) const {
            if (a.estimated_time == b.estimated_time) {
                return a.seq > b.seq;
            }
            return a.estimated_time > b.estimated_time;
        }
    };

    long long seq_ = 0;
    std::priority_queue<Node, std::vector<Node>, Compare> heap_;
};

class PriorityScheduler : public IScheduler {
   public:
    void push(int request_index, const std::vector<Request>& requests, double now) override;
    std::optional<int> pop(const std::vector<Request>& requests, double now) override;
    bool empty() const override;
    std::string name() const override;
    SchedulerType type() const override;

   private:
    struct Node {
        int priority;
        long long seq;
        int request_index;
    };

    struct Compare {
        bool operator()(const Node& a, const Node& b) const {
            if (a.priority == b.priority) {
                return a.seq > b.seq;
            }
            return a.priority > b.priority;
        }
    };

    long long seq_ = 0;
    std::priority_queue<Node, std::vector<Node>, Compare> heap_;
};

class RoundRobinScheduler : public IScheduler {
   public:
    explicit RoundRobinScheduler(double quantum_sec);

    void push(int request_index, const std::vector<Request>& requests, double now) override;
    std::optional<int> pop(const std::vector<Request>& requests, double now) override;
    bool empty() const override;
    std::string name() const override;
    SchedulerType type() const override;

    double quantum_sec() const;

   private:
    double quantum_sec_;
    std::deque<int> queue_;
};

class WfqLikeScheduler : public IScheduler {
   public:
    void push(int request_index, const std::vector<Request>& requests, double now) override;
    std::optional<int> pop(const std::vector<Request>& requests, double now) override;
    bool empty() const override;
    std::string name() const override;
    SchedulerType type() const override;

   private:
    struct Node {
        double finish_tag;
        long long seq;
        int request_index;
    };

    struct Compare {
        bool operator()(const Node& a, const Node& b) const {
            if (a.finish_tag == b.finish_tag) {
                return a.seq > b.seq;
            }
            return a.finish_tag > b.finish_tag;
        }
    };

    static double WeightForPriority(int priority);

    double virtual_time_ = 0.0;
    long long seq_ = 0;
    std::unordered_map<int, double> client_finish_tag_;
    std::priority_queue<Node, std::vector<Node>, Compare> heap_;
};

std::unique_ptr<IScheduler> CreateScheduler(SchedulerType type, double quantum_sec);
std::string SchedulerTypeToString(SchedulerType type);
std::optional<SchedulerType> ParseSchedulerType(const std::string& raw);

}  // namespace simulator

#endif
