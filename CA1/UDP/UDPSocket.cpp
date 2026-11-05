#include "UDPSocket.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/select.h>
#include <unistd.h>

UDPSocket::UDPSocket() : sockfd(-1) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error(std::string("socket failed: ") + std::strerror(errno));
    }
}

UDPSocket::~UDPSocket() {
    if (sockfd >= 0) {
        close(sockfd);
    }
}

void UDPSocket::bind(std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (::bind(sockfd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        throw std::runtime_error(std::string("bind failed: ") + std::strerror(errno));
    }
}

void UDPSocket::sendTo(const std::string& ip, std::uint16_t port, const std::string& data) {
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &destination.sin_addr) != 1) {
        throw std::runtime_error("Invalid IPv4 address: " + ip);
    }

    const ssize_t sent = sendto(sockfd, data.data(), data.size(), 0,
                                reinterpret_cast<sockaddr*>(&destination), sizeof(destination));
    if (sent < 0) {
        throw std::runtime_error(std::string("sendto failed: ") + std::strerror(errno));
    }
    if (static_cast<std::size_t>(sent) != data.size()) {
        throw std::runtime_error("sendto did not send the complete datagram");
    }
}

int UDPSocket::recvFrom(char* buffer, std::size_t max_size, sockaddr_in& sender_addr,
                        int timeout_ms) {
    if (timeout_ms < 0) {
        throw std::invalid_argument("timeout must not be negative");
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(sockfd, &read_set);

    timeval timeout{};
    timeval* timeout_ptr = NULL;
    if (timeout_ms > 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        timeout_ptr = &timeout;
    }

    int ready;
    do {
        fd_set current_set = read_set;
        timeval current_timeout = timeout;
        ready = select(sockfd + 1, &current_set, NULL, NULL,
                       timeout_ptr ? &current_timeout : NULL);
    } while (ready < 0 && errno == EINTR);

    if (ready == 0) {
        return -1;
    }
    if (ready < 0) {
        throw std::runtime_error(std::string("select failed: ") + std::strerror(errno));
    }

    socklen_t address_length = sizeof(sender_addr);
    ssize_t received;
    do {
        address_length = sizeof(sender_addr);
        received = recvfrom(sockfd, buffer, max_size, 0,
                            reinterpret_cast<sockaddr*>(&sender_addr), &address_length);
    } while (received < 0 && errno == EINTR);

    if (received < 0) {
        throw std::runtime_error(std::string("recvfrom failed: ") + std::strerror(errno));
    }

    return static_cast<int>(received);
}
