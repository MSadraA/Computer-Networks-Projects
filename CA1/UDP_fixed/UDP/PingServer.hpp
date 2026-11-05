#ifndef PINGSERVER_HPP
#define PINGSERVER_HPP

#include "UDPSocket.hpp"
#include <cstdint>
#include <random>

struct ServerConfig {
    std::uint16_t port;
    double loss_probability;
    unsigned int seed;
};

class PingServer {
public:
    explicit PingServer(const ServerConfig& config);
    void run();

private:
    ServerConfig config;
    UDPSocket socket;
    std::mt19937 random_engine;
    std::bernoulli_distribution drop_distribution;

    bool shouldDrop();
};

#endif
