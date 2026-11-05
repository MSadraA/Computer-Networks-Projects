#ifndef PINGCLIENT_HPP
#define PINGCLIENT_HPP

#include "UDPSocket.hpp"
#include "PingStats.hpp"
#include <chrono>
#include <string>

struct ClientConfig {
    std::string host;
    uint16_t port;
    int count;
    int timeout_ms;
    int interval_ms;
};

class PingClient {
public:
    PingClient(const ClientConfig& config);
    void run();

private:
    ClientConfig config;
    UDPSocket socket;
    PingStats stats;

    bool waitForPong(uint64_t expected_seq, std::chrono::time_point<std::chrono::steady_clock> start_time, double& out_rtt);
};

#endif