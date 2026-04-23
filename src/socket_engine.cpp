#include "socket_engine.h"
#include <cstring>
#include <stdexcept>
#include <vector>
#include <thread>
#include <chrono>

// POSIX socket headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// ── Loopback server ──────────────────────────────────────────────────────────

// Accept-and-drain thread: reads data from connected clients and discards it.
static void acceptLoop(int server_fd) {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t   addr_len = sizeof(client_addr);
        int client_fd = ::accept(server_fd,
                                 reinterpret_cast<sockaddr*>(&client_addr),
                                 &addr_len);
        if (client_fd < 0) break; // server closed

        // Drain all data and close.
        std::thread([client_fd]() {
            char buf[65536];
            while (true) {
                ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
                if (n <= 0) break;
            }
            ::close(client_fd);
        }).detach();
    }
}

LoopbackServerHandle startLoopbackServer() {
    LoopbackServerHandle h;
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return h;

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0; // OS chooses port

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return h;
    }
    if (::listen(fd, 128) < 0) {
        ::close(fd);
        return h;
    }

    socklen_t len = sizeof(addr);
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    int port = ntohs(addr.sin_port);

    std::thread(acceptLoop, fd).detach();
    h.fd = fd;
    h.port = port;
    return h;
}

void stopLoopbackServer(const LoopbackServerHandle& server) {
    if (server.fd >= 0) ::close(server.fd);
}

// ── Client transfer ──────────────────────────────────────────────────────────

double socketTransfer(int port, double size_mb) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1.0;

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(static_cast<uint16_t>(port));

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1.0;
    }

    // Send size_mb megabytes in 64 KB chunks.
    const size_t chunk = 65536;
    std::vector<char> buf(chunk, 0xAB);
    long long total_bytes = static_cast<long long>(size_mb * 1024.0 * 1024.0);

    auto t0 = std::chrono::steady_clock::now();

    long long sent = 0;
    while (sent < total_bytes) {
        size_t to_send = static_cast<size_t>(
            std::min(static_cast<long long>(chunk), total_bytes - sent));
        ssize_t n = ::send(fd, buf.data(), to_send, 0);
        if (n <= 0) break;
        sent += n;
    }
    ::close(fd);

    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}
