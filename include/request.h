#pragma once
#include <string>
#include <chrono>

// Direction of the file transfer.
enum class TransferDirection { UPLOAD, DOWNLOAD };

// One file-transfer request submitted to the scheduler.
struct TransferJob {
    int    id            = 0;
    int    user_id       = 0;
    double size_mb       = 0.0;   // total transfer size in MB
    TransferDirection direction = TransferDirection::DOWNLOAD;
    int    priority      = 5;     // 1 = highest, 10 = lowest

    // Simulated-time timestamps (milliseconds).
    double arrival_ms    = 0.0;
    double start_ms      = 0.0;
    double completion_ms = 0.0;
    double first_start_ms= 0.0;

    // Remaining size for preemptive Round Robin.
    double remaining_size_mb = 0.0;

    // Virtual finish time for WFQ.
    double virtual_finish_time = 0.0;

    // Helper: direction as string.
    std::string directionStr() const {
        return direction == TransferDirection::UPLOAD ? "UP" : "DL";
    }
};
