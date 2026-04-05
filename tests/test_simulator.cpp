#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "csv_utils.h"
#include "metrics.h"
#include "request.h"
#include "scheduler.h"

namespace {

void Assert(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

void TestSchedulerParsing() {
    using simulator::ParseSchedulerType;
    using simulator::SchedulerType;

    Assert(ParseSchedulerType("fifo").value() == SchedulerType::kFifo, "fifo parse failed");
    Assert(ParseSchedulerType("sjf").value() == SchedulerType::kSjf, "sjf parse failed");
    Assert(ParseSchedulerType("priority").value() == SchedulerType::kPriority, "priority parse failed");
    Assert(ParseSchedulerType("rr").value() == SchedulerType::kRoundRobin, "rr parse failed");
    Assert(ParseSchedulerType("wfq").value() == SchedulerType::kWfq, "wfq parse failed");
    Assert(!ParseSchedulerType("unknown").has_value(), "unknown parser should fail");
}

void TestSchedulerFactory() {
    using simulator::CreateScheduler;
    using simulator::SchedulerType;

    auto fifo = CreateScheduler(SchedulerType::kFifo, 1.0);
    auto rr = CreateScheduler(SchedulerType::kRoundRobin, 0.25);

    Assert(fifo->name() == "fifo", "fifo name mismatch");
    Assert(rr->name() == "rr", "rr name mismatch");
}

void TestMetricsComputation() {
    using simulator::MetricsCollector;
    using simulator::Request;

    std::vector<Request> requests(2);
    requests[0].request_id = 1;
    requests[0].client_id = 1;
    requests[0].arrival_time = 0.0;
    requests[0].file_size_mb = 100.0;
    requests[0].first_start_time = 1.0;
    requests[0].finish_time = 6.0;
    requests[0].total_processing_time = 5.0;

    requests[1].request_id = 2;
    requests[1].client_id = 2;
    requests[1].arrival_time = 0.5;
    requests[1].file_size_mb = 80.0;
    requests[1].first_start_time = 1.5;
    requests[1].finish_time = 5.5;
    requests[1].total_processing_time = 4.0;

    auto rows = MetricsCollector::BuildRequestResults(requests, "fifo");
    Assert(rows.size() == 2, "rows size mismatch");

    auto summary = MetricsCollector::BuildSummary(requests, "fifo", 6.0);
    Assert(std::fabs(summary.avg_waiting_time - 1.0) < 1e-9, "avg waiting mismatch");
    Assert(std::fabs(summary.avg_response_time - 1.0) < 1e-9, "avg response mismatch");
    Assert(summary.fairness_index > 0.0 && summary.fairness_index <= 1.0, "fairness out of range");
}

void TestCsvReadAndGenerate() {
    using simulator::GenerateSampleRequestsCsv;
    using simulator::ReadRequestsCsv;

    const std::filesystem::path temp = std::filesystem::temp_directory_path() / "simulator_test_requests.csv";
    GenerateSampleRequestsCsv(temp.string(), 12, 123);
    auto requests = ReadRequestsCsv(temp.string());

    Assert(requests.size() == 12, "generated CSV row count mismatch");
    Assert(requests.front().request_id == 1, "first request id mismatch");
    std::filesystem::remove(temp);
}

}  // namespace

int main() {
    try {
        TestSchedulerParsing();
        TestSchedulerFactory();
        TestMetricsComputation();
        TestCsvReadAndGenerate();
        std::cout << "All tests passed.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }
}
