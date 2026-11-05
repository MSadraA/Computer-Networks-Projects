#include "PingClient.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage() {
    std::cerr << "Usage: ./ping-client --host H --port P [--count N] [--timeout MS] [--interval MS]\n";
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

}

int main(int argc, char* argv[]) {
    ClientConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.count = 10;
    config.timeout_ms = 1000;
    config.interval_ms = 0;
    bool port_set = false;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--host" || option == "--port" || option == "--count" ||
                option == "--timeout" || option == "--interval") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("Missing value for " + option);
                }
                const std::string value = argv[++i];
                if (option == "--host") {
                    config.host = value;
                } else {
                    const long long number = parseInteger(value, option);
                    if (option == "--port") {
                        if (number < 1 || number > 65535) {
                            throw std::invalid_argument("--port must be between 1 and 65535");
                        }
                        config.port = static_cast<std::uint16_t>(number);
                        port_set = true;
                    } else if (option == "--count") {
                        if (number < 1 || number > 1000000) {
                            throw std::invalid_argument("--count must be between 1 and 1000000");
                        }
                        config.count = static_cast<int>(number);
                    } else if (option == "--timeout") {
                        if (number < 1 || number > 3600000) {
                            throw std::invalid_argument("--timeout must be between 1 and 3600000 ms");
                        }
                        config.timeout_ms = static_cast<int>(number);
                    } else {
                        if (number < 0 || number > 3600000) {
                            throw std::invalid_argument("--interval must be between 0 and 3600000 ms");
                        }
                        config.interval_ms = static_cast<int>(number);
                    }
                }
            } else {
                throw std::invalid_argument("Unknown option: " + option);
            }
        }

        if (!port_set) {
            printUsage();
            return 1;
        }

        PingClient client(config);
        client.run();
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
