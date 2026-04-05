#pragma once
// Extension 1: Real socket-based transfer engine.
// When --engine socket is passed, Server uses SocketEngine to perform
// actual TCP loopback transfers instead of sleeping.

#include <string>
#include <functional>

// Starts a local TCP echo server on loopback.
// Returns the port number selected (0 = failure).
int startLoopbackServer();

// Transfers 'size_mb' megabytes via a TCP connection to localhost:port.
// Returns the actual wall-clock duration in milliseconds.
double socketTransfer(int port, double size_mb);

// Stops the loopback server (closes the listening socket).
void stopLoopbackServer(int server_fd);
