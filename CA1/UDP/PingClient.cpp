#include "PingClient.hpp"
#include "MessageParser.hpp"
#include <iostream>
#include <thread>

PingClient::PingClient(const ClientConfig& cfg) : config(cfg), stats(cfg.count) {}

void PingClient::run() {
    for (int i = 1; i <= config.count; ++i) {
        std::string ping_msg = MessageParser::serializePing(i, "my_token");
        
        auto start_time = std::chrono::steady_clock::now();
        socket.sendTo(config.host, config.port, ping_msg);

        double rtt = 0.0;
        bool received = waitForPong(i, start_time, rtt);

        if (received) {
            stats.recordRtt(rtt);
            std::cout << "PONG seq=" << i << " rtt=" << rtt << " ms\n";
        } else {
            stats.recordTimeout();
            std::cout << "TIMEOUT seq=" << i << "\n";
        }

        if (i < config.count && config.interval_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config.interval_ms));
        }
    }
    stats.printFinalReport();
}

bool PingClient::waitForPong(uint64_t expected_seq, std::chrono::time_point<std::chrono::steady_clock> start_time, double& out_rtt) {
    char buffer[udp_pinger::MAX_PAYLOAD_SIZE + 1];
    sockaddr_in server_addr{};

    while (true) {
        auto now = std::chrono::steady_clock::now();
        int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        int time_left = config.timeout_ms - elapsed_ms;
        
        if (time_left <= 0) return false;

        int bytes = socket.recvFrom(buffer, udp_pinger::MAX_PAYLOAD_SIZE, server_addr, time_left);
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            udp_pinger::Message msg = MessageParser::deserialize(buffer);
            
            if (msg.type == udp_pinger::MessageType::PONG && msg.seq == expected_seq) {
                auto end_wait = std::chrono::steady_clock::now();
                std::chrono::duration<double, std::milli> duration = end_wait - start_time;
                out_rtt = duration.count();
                return true;
            }
        } else if (bytes == -1) {
            return false;
        }
    }
}