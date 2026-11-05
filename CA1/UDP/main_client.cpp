#include "PingClient.hpp"
#include <iostream>
#include <string>
#include <cstring>

void printUsage() {
    std::cerr << "Usage: ./ping-client --host H --port P [--count N] [--timeout MS] [--interval MS]\n";
}

int main(int argc, char* argv[]) {
    ClientConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.count = 10;
    config.timeout_ms = 1000;
    config.interval_ms = 1000;

    bool port_set = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            config.host = argv[++i];
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            port_set = true;
        } else if (std::strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            config.count = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            config.timeout_ms = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--interval") == 0 && i + 1 < argc) {
            config.interval_ms = std::stoi(argv[++i]);
        }
    }

    if (!port_set) {
        printUsage();
        return 1;
    }

    try {
        PingClient client(config);
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}