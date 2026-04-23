#include "web_dashboard.h"
#include "request.h"
#include <sstream>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <iomanip>

// POSIX socket headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

// ── helpers ───────────────────────────────────────────────────────────────────

static void writeAll(int fd, const std::string& s) {
    size_t offset = 0;
    while (offset < s.size()) {
        ssize_t n = ::send(fd, s.data() + offset, s.size() - offset, MSG_NOSIGNAL);
        if (n <= 0) break;
        offset += static_cast<size_t>(n);
    }
}

// ── HTML page ─────────────────────────────────────────────────────────────────

std::string WebDashboard::htmlPage() {
    return R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Scheduler Simulator Dashboard</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4"></script>
<style>
  body { font-family: sans-serif; background: #1a1a2e; color: #eee; margin: 20px; }
  h1   { color: #e94560; }
  .charts { display: flex; flex-wrap: wrap; gap: 20px; }
  .chart-box { background: #16213e; padding: 16px; border-radius: 8px;
               width: 480px; height: 320px; }
</style>
</head>
<body>
<h1>&#x1F4C8; Scheduler Simulator — Live Dashboard</h1>
<p id="status">Waiting for events&#x2026;</p>
<div class="charts">
  <div class="chart-box"><canvas id="waitChart"></canvas></div>
  <div class="chart-box"><canvas id="tatChart"></canvas></div>
</div>
<script>
const waitData = {labels:[], datasets:[{label:'Wait (ms)',data:[],
  backgroundColor:'#e94560', borderColor:'#e94560', fill:false}]};
const tatData  = {labels:[], datasets:[{label:'TAT (ms)',data:[],
  backgroundColor:'#0f3460', borderColor:'#0f3460', fill:false}]};

const opts = {animation:false, responsive:true, maintainAspectRatio:false,
  scales:{y:{beginAtZero:true}}};

const waitChart = new Chart(document.getElementById('waitChart'),
  {type:'bar', data:waitData, options:opts});
const tatChart  = new Chart(document.getElementById('tatChart'),
  {type:'bar', data:tatData,  options:opts});

const src = new EventSource('/events');
src.addEventListener('job', e => {
  const d = JSON.parse(e.data);
  document.getElementById('status').textContent =
    `Received ${waitData.labels.length+1} jobs`;
  const label = `#${d.id}`;
  waitData.labels.push(label);
  waitData.datasets[0].data.push(d.wait_ms);
  tatData.labels.push(label);
  tatData.datasets[0].data.push(d.tat_ms);
  waitChart.update();
  tatChart.update();
});
src.addEventListener('done', () => {
  document.getElementById('status').textContent = 'Simulation complete.';
  src.close();
});
</script>
</body>
</html>
)html";
}

// ── SSE event ────────────────────────────────────────────────────────────────

std::string WebDashboard::sseEvent(const TransferJob& job, const std::string& policy) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(2);
    o << "event: job\ndata: {"
      << "\"id\":"       << job.id
      << ",\"policy\":\"" << policy << "\""
      << ",\"wait_ms\":" << (job.start_ms - job.arrival_ms)
      << ",\"tat_ms\":"  << (job.completion_ms - job.arrival_ms)
      << ",\"size_mb\":" << job.size_mb
      << "}\n\n";
    return o.str();
}

// ── WebDashboard ──────────────────────────────────────────────────────────────

WebDashboard::WebDashboard(int port) : port_(port) {}

WebDashboard::~WebDashboard() { stop(); }

void WebDashboard::start() {
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "web_dashboard: socket() failed\n";
        return;
    }
    int opt = 1;
    ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "web_dashboard: bind() failed on port " << port_ << "\n";
        ::close(server_fd_);
        server_fd_ = -1;
        return;
    }
    ::listen(server_fd_, 32);
    running_ = true;

    accept_thread_ = std::thread(&WebDashboard::acceptLoop, this);
    std::cout << "[web] Dashboard running at http://localhost:" << port_ << "/\n";
}

void WebDashboard::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        // Send "done" event to all clients.
        {
            std::unique_lock<std::mutex> lock(clients_mutex_);
            for (int fd : client_fds_)
                writeAll(fd, "event: done\ndata: {}\n\n");
        }
        ::shutdown(server_fd_, SHUT_RDWR);
        ::close(server_fd_);
        server_fd_ = -1;
    }
    if (accept_thread_.joinable()) accept_thread_.join();

    std::unique_lock<std::mutex> lock(clients_mutex_);
    for (int fd : client_fds_) ::close(fd);
    client_fds_.clear();
}

void WebDashboard::push(const TransferJob& job, const std::string& policy) {
    std::string evt = sseEvent(job, policy);
    std::unique_lock<std::mutex> lock(clients_mutex_);
    // Broadcast to all SSE clients; remove disconnected ones.
    client_fds_.erase(
        std::remove_if(client_fds_.begin(), client_fds_.end(),
            [&](int fd) {
                ssize_t n = ::send(fd, evt.data(), evt.size(), MSG_NOSIGNAL);
                return n <= 0;
            }),
        client_fds_.end());
}

void WebDashboard::acceptLoop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int cfd = ::accept(server_fd_,
                           reinterpret_cast<sockaddr*>(&client_addr), &len);
        if (cfd < 0) break;
        std::thread(&WebDashboard::handleClient, this, cfd).detach();
    }
}

void WebDashboard::handleClient(int client_fd) {
    // Read HTTP request (just enough to identify path).
    char buf[4096] = {};
    ssize_t n = ::recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { ::close(client_fd); return; }

    std::string req(buf, static_cast<size_t>(n));
    bool is_events = req.find("GET /events") != std::string::npos;

    if (is_events) {
        // SSE endpoint.
        std::string hdr =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n";
        writeAll(client_fd, hdr);

        // Register this client for broadcasts.
        {
            std::unique_lock<std::mutex> lock(clients_mutex_);
            client_fds_.push_back(client_fd);
        }
        // Keep thread alive until client disconnects or server stops.
        while (running_) {
            char ping[1];
            ssize_t r = ::recv(client_fd, ping, 1, MSG_PEEK | MSG_DONTWAIT);
            if (r == 0) break; // client closed
            if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        // Remove from list.
        std::unique_lock<std::mutex> lock(clients_mutex_);
        client_fds_.erase(
            std::remove(client_fds_.begin(), client_fds_.end(), client_fd),
            client_fds_.end());
    } else {
        // Serve static HTML page.
        std::string body = htmlPage();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/html; charset=UTF-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
        writeAll(client_fd, resp.str());
    }
    ::close(client_fd);
}
