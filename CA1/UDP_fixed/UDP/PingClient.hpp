#ifndef PINGCLIENT_HPP
#define PINGCLIENT_HPP

#include "UDPSocket.hpp"
#include "PingStats.hpp"
#include <chrono>
#include <cstdint>
#include <string>

struct ClientConfig {
    std::string host;
    std::uint16_t port;
    int count;
    int timeout_ms;
    int interval_ms;
};

class PingClient {
public:
    explicit PingClient(const ClientConfig& config);
    void run();

private:
    ClientConfig config;
    UDPSocket socket;
    PingStats stats;

    bool waitForPong(std::uint64_t expected_seq, const std::string& expected_token,
                     const sockaddr_in& expected_server,
                     std::chrono::steady_clock::time_point start_time, double& out_rtt);
};

#endif
