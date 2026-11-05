#include "PingServer.hpp"
#include <iostream>
#include <string>
#include <cstring>

void printUsage() {
    std::cerr << "Usage: ./ping-server --port P --loss L [--seed S]\n";
}

int main(int argc, char* argv[]) {
    ServerConfig config;
    config.port = 0;
    config.loss_probability = 0.0f;
    config.seed = 0;

    bool port_set = false;
    bool loss_set = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            port_set = true;
        } else if (std::strcmp(argv[i], "--loss") == 0 && i + 1 < argc) {
            config.loss_probability = std::stof(argv[++i]);
            loss_set = true;
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            config.seed = static_cast<unsigned int>(std::stoul(argv[++i]));
        }
    }

    if (!port_set || !loss_set) {
        printUsage();
        return 1;
    }

    try {
        PingServer server(config);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}