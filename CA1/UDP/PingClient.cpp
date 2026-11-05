#include "PingClient.hpp"
#include "MessageParser.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>

namespace {
const std::string CLIENT_TOKEN = "my_token";
}

PingClient::PingClient(const ClientConfig& cfg) : config(cfg) {}

void PingClient::run() {
    sockaddr_in expected_server{};
    expected_server.sin_family = AF_INET;
    expected_server.sin_port = htons(config.port);
    if (inet_pton(AF_INET, config.host.c_str(), &expected_server.sin_addr) != 1) {
        throw std::runtime_error("Invalid IPv4 address: " + config.host);
    }

    for (int sequence = 1; sequence <= config.count; ++sequence) {
        const std::string ping = MessageParser::serializePing(sequence, CLIENT_TOKEN);
        const std::chrono::steady_clock::time_point start_time =
            std::chrono::steady_clock::now();

        socket.sendTo(config.host, config.port, ping);

        double rtt = 0.0;
        if (waitForPong(static_cast<std::uint64_t>(sequence), CLIENT_TOKEN,
                        expected_server, start_time, rtt)) {
            stats.recordRtt(rtt);
            std::cout << "PONG seq=" << sequence << " rtt="
                      << std::fixed << std::setprecision(3) << rtt << " ms\n";
        } else {
            stats.recordTimeout();
            std::cout << "TIMEOUT seq=" << sequence << "\n";
        }

        if (sequence < config.count && config.interval_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config.interval_ms));
        }
    }

    stats.printFinalReport();
}

bool PingClient::waitForPong(std::uint64_t expected_seq, const std::string& expected_token,
                             const sockaddr_in& expected_server,
                             std::chrono::steady_clock::time_point start_time, double& out_rtt) {
    char buffer[udp_pinger::MAX_PAYLOAD_SIZE + 1];

    while (true) {
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const std::chrono::steady_clock::duration elapsed = now - start_time;
        const std::chrono::steady_clock::duration allowed =
            std::chrono::milliseconds(config.timeout_ms);

        if (elapsed >= allowed) {
            return false;
        }

        const std::chrono::milliseconds remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(allowed - elapsed);
        int wait_ms = static_cast<int>(remaining.count());
        if (wait_ms == 0) wait_ms = 1;

        sockaddr_in sender{};
        const int bytes = socket.recvFrom(buffer, udp_pinger::MAX_PAYLOAD_SIZE, sender, wait_ms);
        if (bytes == -1) {
            // select may round the remaining time down by a fraction of a millisecond.
            // Check the deadline again before deciding that the request timed out.
            continue;
        }
        if (bytes <= 0) {
            continue;
        }

        if (sender.sin_family != expected_server.sin_family ||
            sender.sin_addr.s_addr != expected_server.sin_addr.s_addr ||
            sender.sin_port != expected_server.sin_port) {
            continue;
        }

        const udp_pinger::Message message =
            MessageParser::deserialize(std::string(buffer, static_cast<std::size_t>(bytes)));

        if (message.type == udp_pinger::MessageType::PONG &&
            message.seq == expected_seq && message.token == expected_token) {
            const std::chrono::steady_clock::time_point received_at =
                std::chrono::steady_clock::now();
            const std::chrono::duration<double, std::milli> rtt = received_at - start_time;
            out_rtt = rtt.count();
            return true;
        }
        // Old, malformed, duplicate, or unrelated replies are ignored until timeout.
    }
}
