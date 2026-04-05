#pragma once
// Extension 6: Web dashboard — real-time browser visualization via
// Server-Sent Events (SSE) over a lightweight embedded HTTP server.

#include "metrics.h"
#include <string>
#include <thread>
#include <atomic>
#include <functional>

// A minimal HTTP server that:
//  1. Serves a static HTML page with Chart.js charts at GET /
//  2. Streams job-completion events via SSE at GET /events
//
// Usage:
//   WebDashboard dash(8080);
//   dash.start();
//   // ... simulation runs ...
//   dash.push(job);         // call from any thread as jobs complete
//   dash.stop();

class WebDashboard {
public:
    explicit WebDashboard(int port = 8080);
    ~WebDashboard();

    void start();
    void stop();

    // Push a completed job as an SSE event to all connected clients.
    void push(const TransferJob& job, const std::string& policy);

    int port() const { return port_; }

private:
    void acceptLoop();
    void handleClient(int client_fd);
    static std::string htmlPage();
    static std::string sseEvent(const TransferJob& job, const std::string& policy);

    int               port_;
    int               server_fd_ = -1;
    std::thread       accept_thread_;
    std::atomic<bool> running_{false};

    mutable std::mutex  clients_mutex_;
    std::vector<int>    client_fds_;
};
