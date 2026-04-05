#ifndef METRICS_H
#define METRICS_H

#include <string>
#include <vector>

#include "request.h"

namespace simulator {

class MetricsCollector {
   public:
    static std::vector<RequestResult> BuildRequestResults(
        const std::vector<Request>& requests, const std::string& scheduler_name);

    static MetricsSummary BuildSummary(
        const std::vector<Request>& requests,
        const std::string& scheduler_name,
        double total_completion_time);
};

}  // namespace simulator

#endif
