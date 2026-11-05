#include "UDPSocket.hpp"
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <sys/select.h>

UDPSocket::UDPSocket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) throw std::runtime_error("Failed to create socket");
}

UDPSocket::~UDPSocket() {
    close(sockfd);
}

void UDPSocket::bind(uint16_t port) {
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (::bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Bind failed");
    }
}

void UDPSocket::sendTo(const std::string& ip, uint16_t port, const std::string& data) {
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &dest_addr.sin_addr);

    sendto(sockfd, data.c_str(), data.length(), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
}

int UDPSocket::recvFrom(char* buffer, size_t max_size, sockaddr_in& sender_addr, int timeout_ms) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int activity = select(sockfd + 1, &readfds, NULL, NULL, timeout_ms > 0 ? &tv : NULL);
    
    if (activity == 0) return -1; // Timeout
    if (activity < 0) return -2;  // Error

    socklen_t addr_len = sizeof(sender_addr);
    return recvfrom(sockfd, buffer, max_size, 0, (struct sockaddr*)&sender_addr, &addr_len);
}