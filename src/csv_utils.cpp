#include "csv_utils.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace simulator {

namespace {

std::vector<std::string> SplitCsv(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(line);
    while (std::getline(ss, token, ',')) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string Trim(const std::string& s) {
    std::size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])) != 0) {
        ++begin;
    }
    std::size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }
    return s.substr(begin, end - begin);
}

void EnsureParentDir(const std::string& path) {
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
}

}  // namespace

std::vector<Request> ReadRequestsCsv(const std::string& path) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        throw std::runtime_error("Cannot open input file: " + path);
    }

    std::string header_line;
    if (!std::getline(fin, header_line)) {
        return {};
    }

    std::vector<std::string> headers = SplitCsv(header_line);
    std::unordered_map<std::string, std::size_t> idx;
    for (std::size_t i = 0; i < headers.size(); ++i) {
        idx[Trim(headers[i])] = i;
    }

    auto require = [&idx](const std::string& key) -> std::size_t {
        auto it = idx.find(key);
        if (it == idx.end()) {
            throw std::runtime_error("Missing required CSV column: " + key);
        }
        return it->second;
    };

    std::size_t id_col = require("request_id");
    std::size_t arrival_col = require("arrival_time");
    std::size_t size_col = require("file_size_mb");
    std::size_t bw_col = require("estimated_bandwidth_mb_per_sec");
    std::size_t priority_col = require("priority");

    bool has_client = idx.find("client_id") != idx.end();
    std::size_t client_col = has_client ? idx["client_id"] : 0;

    std::vector<Request> requests;
    std::string line;
    while (std::getline(fin, line)) {
        if (Trim(line).empty()) {
            continue;
        }

        std::vector<std::string> row = SplitCsv(line);
        const std::size_t expected_cols = headers.size();
        if (row.size() < expected_cols) {
            row.resize(expected_cols);
        }

        Request req;
        req.request_id = std::stoi(Trim(row[id_col]));
        req.arrival_time = std::stod(Trim(row[arrival_col]));
        req.file_size_mb = std::stod(Trim(row[size_col]));
        req.estimated_bandwidth_mb_per_sec = std::stod(Trim(row[bw_col]));
        req.priority = std::stoi(Trim(row[priority_col]));
        req.client_id = has_client ? std::stoi(Trim(row[client_col])) : req.request_id;
        req.remaining_size_mb = req.file_size_mb;
        requests.push_back(req);
    }

    std::sort(requests.begin(), requests.end(), [](const Request& a, const Request& b) {
        if (a.arrival_time == b.arrival_time) {
            return a.request_id < b.request_id;
        }
        return a.arrival_time < b.arrival_time;
    });

    return requests;
}

void WriteRequestResultsCsv(const std::string& path, const std::vector<RequestResult>& rows) {
    EnsureParentDir(path);
    std::ofstream fout(path);
    if (!fout.is_open()) {
        throw std::runtime_error("Cannot write file: " + path);
    }

    fout << "request_id,client_id,arrival_time,start_time,finish_time,waiting_time,turnaround_time,response_time,assigned_thread,scheduler_name\n";
    fout << std::fixed << std::setprecision(4);

    for (const auto& row : rows) {
        fout << row.request_id << ',' << row.client_id << ',' << row.arrival_time << ',' << row.start_time << ','
             << row.finish_time << ',' << row.waiting_time << ',' << row.turnaround_time << ','
             << row.response_time << ',' << row.assigned_thread << ',' << row.scheduler_name << '\n';
    }
}

void WriteSummaryCsv(const std::string& path, const std::vector<MetricsSummary>& rows) {
    EnsureParentDir(path);
    std::ofstream fout(path);
    if (!fout.is_open()) {
        throw std::runtime_error("Cannot write file: " + path);
    }

    fout << "scheduler,avg_waiting_time,avg_turnaround_time,avg_response_time,throughput,fairness_index,total_completion_time\n";
    fout << std::fixed << std::setprecision(6);

    for (const auto& row : rows) {
        fout << row.scheduler << ',' << row.avg_waiting_time << ',' << row.avg_turnaround_time << ','
             << row.avg_response_time << ',' << row.throughput << ',' << row.fairness_index << ','
             << row.total_completion_time << '\n';
    }
}

void GenerateSampleRequestsCsv(const std::string& path, std::size_t n, unsigned int seed) {
    EnsureParentDir(path);

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> client_dist(1, 12);
    std::uniform_real_distribution<double> arrival_step_dist(0.0, 1.6);
    std::uniform_real_distribution<double> file_size_dist(10.0, 900.0);
    std::uniform_real_distribution<double> bw_dist(5.0, 80.0);
    std::uniform_int_distribution<int> priority_dist(1, 10);

    std::ofstream fout(path);
    if (!fout.is_open()) {
        throw std::runtime_error("Cannot write file: " + path);
    }

    fout << "request_id,client_id,arrival_time,file_size_mb,estimated_bandwidth_mb_per_sec,priority\n";
    fout << std::fixed << std::setprecision(4);

    double arrival = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        arrival += arrival_step_dist(rng);
        fout << (i + 1) << ',' << client_dist(rng) << ',' << arrival << ',' << file_size_dist(rng) << ','
             << bw_dist(rng) << ',' << priority_dist(rng) << '\n';
    }
}

}  // namespace simulator
