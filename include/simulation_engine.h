#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include <string>
#include <vector>

#include "request.h"
#include "scheduler.h"

namespace simulator {

struct SimulationConfig {
    std::string input_path;
    std::string output_dir = "output";
    SchedulerType scheduler = SchedulerType::kFifo;
    bool benchmark = false;
    int threads = 4;
    double quantum_sec = 1.0;
    unsigned int seed = 42;
    double connection_overhead_sec = 0.05;
    bool verbose = true;
};

class SimulationEngine {
   public:
    explicit SimulationEngine(SimulationConfig config);

    int Run();
    static int GenerateSample(const std::string& output_path, std::size_t n, unsigned int seed);

   private:
    struct RunOutput {
        MetricsSummary summary;
        std::vector<RequestResult> rows;
    };

    RunOutput RunOneScheduler(SchedulerType type, const std::vector<Request>& original_requests) const;
    void PrintConsoleSummary(const MetricsSummary& summary) const;
    void PrintBenchmarkSummary(const std::vector<MetricsSummary>& summaries) const;

    SimulationConfig config_;
};

}  // namespace simulator

#endif
