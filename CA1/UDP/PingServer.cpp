#include "PingServer.hpp"
#include "MessageParser.hpp"

#include <arpa/inet.h>
#include <iostream>

PingServer::PingServer(const ServerConfig& cfg)
    : config(cfg), random_engine(cfg.seed), drop_distribution(cfg.loss_probability) {
    socket.bind(config.port);
}

bool PingServer::shouldDrop() {
    return drop_distribution(random_engine);
}

void PingServer::run() {
    std::cout << "Server listening on port " << config.port << "...\n";

    // One extra byte lets us detect datagrams that exceed the 512-byte limit.
    char buffer[udp_pinger::MAX_PAYLOAD_SIZE + 1];

    while (true) {
        sockaddr_in client_address{};
        const int bytes_read = socket.recvFrom(buffer, sizeof(buffer), client_address);
        if (bytes_read < 0) {
            continue;
        }

        if (static_cast<std::size_t>(bytes_read) > udp_pinger::MAX_PAYLOAD_SIZE) {
            const std::string client_ip = inet_ntoa(client_address.sin_addr);
            socket.sendTo(client_ip, ntohs(client_address.sin_port), udp_pinger::ERR_BAD_REQUEST);
            continue;
        }

        const std::string request(buffer, static_cast<std::size_t>(bytes_read));
        const udp_pinger::Message message = MessageParser::deserialize(request);
        const std::string client_ip = inet_ntoa(client_address.sin_addr);
        const std::uint16_t client_port = ntohs(client_address.sin_port);

        if (message.type != udp_pinger::MessageType::PING) {
            socket.sendTo(client_ip, client_port, udp_pinger::ERR_BAD_REQUEST);
            continue;
        }

        // Invalid requests are answered before packet loss is simulated.
        if (shouldDrop()) {
            continue;
        }

        socket.sendTo(client_ip, client_port,
                      MessageParser::serializePong(message.seq, message.token));
    }
}
