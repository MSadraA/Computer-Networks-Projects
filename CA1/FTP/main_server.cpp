#include "FTPServer.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

std::uint16_t parsePort(const std::string& value) {
    if (value.empty()) throw std::invalid_argument("Invalid port: " + value);
    for (char digit : value) {
        if (digit < '0' || digit > '9') {
            throw std::invalid_argument("Invalid port: " + value);
        }
    }
    const unsigned long parsed = std::stoul(value, nullptr, 10);
    if (parsed == 0 || parsed > 65535) {
        throw std::invalid_argument("Invalid port: " + value);
    }
    return static_cast<std::uint16_t>(parsed);
}

}

int main(int argc, char* argv[]) {
    ServerConfig config{};
    bool port_set = false;
    bool root_set = false;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option != "--port" && option != "--root") {
                throw std::invalid_argument("Unknown option: " + option);
            }
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + option);
            }

            const std::string value = argv[++i];
            if (option == "--port") {
                config.port = parsePort(value);
                port_set = true;
            } else {
                config.root_dir = value;
                root_set = true;
            }
        }

        if (!port_set || !root_set) {
            std::cerr << "Usage: ./ftp-server --port P --root DIR\n";
            return 1;
        }

        FTPServer server(config);
        server.run();
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
