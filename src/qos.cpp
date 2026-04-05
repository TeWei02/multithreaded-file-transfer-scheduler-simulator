#include "qos.h"
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <algorithm>

std::string phbName(DiffServPHB phb) {
    switch (phb) {
        case DiffServPHB::EF:  return "EF";
        case DiffServPHB::AF1: return "AF1";
        case DiffServPHB::BE:  return "BE";
    }
    return "?";
}

DiffServPHB classifyUser(int user_id) {
    if (user_id >= 1 && user_id <= 3)  return DiffServPHB::EF;
    if (user_id >= 4 && user_id <= 6)  return DiffServPHB::AF1;
    return DiffServPHB::BE;
}

int phbPriority(DiffServPHB phb) {
    switch (phb) {
        case DiffServPHB::EF:  return 1;
        case DiffServPHB::AF1: return 5;
        case DiffServPHB::BE:  return 9;
    }
    return 9;
}

std::vector<QoSClassStats> computeQoSStats(const std::vector<TransferJob>& jobs,
                                            double sim_duration_ms) {
    std::unordered_map<int, QoSClassStats> map;

    for (auto phb : { DiffServPHB::EF, DiffServPHB::AF1, DiffServPHB::BE }) {
        QoSClassStats s;
        s.phb = phb;
        map[static_cast<int>(phb)] = s;
    }

    for (const auto& j : jobs) {
        DiffServPHB phb = classifyUser(j.user_id);
        auto& s = map[static_cast<int>(phb)];
        s.avg_wait_ms += j.start_ms - j.arrival_ms;
        s.avg_tat_ms  += j.completion_ms - j.arrival_ms;
        ++s.count;
    }

    std::vector<QoSClassStats> result;
    for (auto phb : { DiffServPHB::EF, DiffServPHB::AF1, DiffServPHB::BE }) {
        auto& s = map[static_cast<int>(phb)];
        s.phb = phb;
        if (s.count > 0) {
            s.avg_wait_ms /= s.count;
            s.avg_tat_ms  /= s.count;
        }
        if (sim_duration_ms > 0.0)
            s.throughput = s.count / (sim_duration_ms / 1000.0);
        result.push_back(s);
    }
    return result;
}

void printQoSStats(const std::vector<QoSClassStats>& stats) {
    std::cout << "\n--- DiffServ QoS Class Statistics ---\n";
    std::cout << std::left  << std::setw(8)  << "PHB"
              << std::right << std::setw(8)  << "Count"
              << std::setw(14) << "AvgWait(ms)"
              << std::setw(13) << "AvgTAT(ms)"
              << std::setw(12) << "Throughput"
              << "\n"
              << std::string(55, '-') << "\n";

    for (const auto& s : stats) {
        std::cout << std::fixed << std::setprecision(1)
                  << std::left  << std::setw(8) << phbName(s.phb)
                  << std::right << std::setw(8) << s.count
                  << std::setw(14) << s.avg_wait_ms
                  << std::setw(13) << s.avg_tat_ms
                  << std::setw(12) << std::setprecision(3) << s.throughput
                  << "\n";
    }
    std::cout << "\n";
}
