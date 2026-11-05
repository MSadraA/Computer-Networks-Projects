#include "PingServer.hpp"

#include <cmath>
#include <ctime>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage() {
    std::cerr << "Usage: ./ping-server --port P [--loss L] [--seed S]\n";
}

long long parseInteger(const std::string& value, const std::string& option) {
    std::size_t used = 0;
    long long result;
    try {
        result = std::stoll(value, &used);
    } catch (...) {
        throw std::invalid_argument("Invalid value for " + option + ": " + value);
    }
    if (used != value.size()) {
        throw std::invalid_argument("Invalid value for " + option + ": " + value);
    }
    return result;
}

double parseLoss(const std::string& value) {
    std::size_t used = 0;
    double result;
    try {
        result = std::stod(value, &used);
    } catch (...) {
        throw std::invalid_argument("Invalid value for --loss: " + value);
    }
    if (used != value.size() || !std::isfinite(result) || result < 0.0 || result > 1.0) {
        throw std::invalid_argument("--loss must be a number between 0.0 and 1.0");
    }
    return result;
}

}

int main(int argc, char* argv[]) {
    ServerConfig config;
    config.port = 0;
    config.loss_probability = 0.0;
    config.seed = static_cast<unsigned int>(std::time(NULL));
    bool port_set = false;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--port" || option == "--loss" || option == "--seed") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("Missing value for " + option);
                }
                const std::string value = argv[++i];
                if (option == "--port") {
                    const long long port = parseInteger(value, option);
                    if (port < 1 || port > 65535) {
                        throw std::invalid_argument("--port must be between 1 and 65535");
                    }
                    config.port = static_cast<std::uint16_t>(port);
                    port_set = true;
                } else if (option == "--loss") {
                    config.loss_probability = parseLoss(value);
                } else {
                    const long long seed = parseInteger(value, option);
                    if (seed < 0 || static_cast<unsigned long long>(seed) > 0xFFFFFFFFULL) {
                        throw std::invalid_argument("--seed must be between 0 and 4294967295");
                    }
                    config.seed = static_cast<unsigned int>(seed);
                }
            } else {
                throw std::invalid_argument("Unknown option: " + option);
            }
        }

        if (!port_set) {
            printUsage();
            return 1;
        }

        PingServer server(config);
        server.run();
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
