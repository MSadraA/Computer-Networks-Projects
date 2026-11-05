#include "PingServer.hpp"
#include "MessageParser.hpp"
#include <iostream>
#include <cstdlib>
#include <arpa/inet.h>

PingServer::PingServer(const ServerConfig& cfg) : config(cfg) {
    if (config.seed != 0) srand(config.seed);
    socket.bind(config.port);
}

bool PingServer::shouldDrop() const {
    float rand_val = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return rand_val < config.loss_probability;
}

void PingServer::run() {
    std::cout << "Server listening on port " << config.port << "...\n";
    char buffer[udp_pinger::MAX_PAYLOAD_SIZE + 1];

    while (true) {
        sockaddr_in client_addr{};
        int bytes_read = socket.recvFrom(buffer, udp_pinger::MAX_PAYLOAD_SIZE, client_addr, 0);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            uint16_t client_port = ntohs(client_addr.sin_port);

            udp_pinger::Message msg = MessageParser::deserialize(buffer);

            // send error message if the message type is UNKNOWN
            if (msg.type == udp_pinger::MessageType::UNKNOWN) {
                socket.sendTo(client_ip, client_port, udp_pinger::ERR_BAD_REQUEST);
                continue;
            }

            if (msg.type == udp_pinger::MessageType::PING) {
                if (shouldDrop()) continue; // Simulate packet loss
                
                std::string pong = MessageParser::serializePong(msg.seq, msg.token);
                socket.sendTo(client_ip, client_port, pong);
            }
        }
    }
}