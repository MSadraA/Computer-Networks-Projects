#include "TCPSocket.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/select.h>
#include <unistd.h>

namespace {
std::runtime_error socketError(const std::string& operation) {
    return std::runtime_error(operation + ": " + std::strerror(errno));
}
}

TCPSocket::TCPSocket() : sockfd(::socket(AF_INET, SOCK_STREAM, 0)) {
    if (sockfd < 0) throw socketError("socket");
    int enabled = 1;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
        int saved_errno = errno;
        ::close(sockfd);
        sockfd = -1;
        errno = saved_errno;
        throw socketError("setsockopt");
    }
}

TCPSocket::TCPSocket(int existing_fd) : sockfd(existing_fd) {}

TCPSocket::~TCPSocket() {
    close();
}

void TCPSocket::close() noexcept {
    if (sockfd >= 0) {
        const int descriptor = sockfd;
        sockfd = -1;
        ::close(descriptor);
    }
}

TCPSocket::TCPSocket(TCPSocket&& other) noexcept : sockfd(other.sockfd) {
    other.sockfd = -1;
}

TCPSocket& TCPSocket::operator=(TCPSocket&& other) noexcept {
    if (this != &other) {
        if (sockfd >= 0) ::close(sockfd);
        sockfd = other.sockfd;
        other.sockfd = -1;
    }
    return *this;
}

void TCPSocket::bind(std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (::bind(sockfd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        throw socketError("bind");
    }
}

void TCPSocket::bindToAny() {
    bind(0);
}

std::uint16_t TCPSocket::getLocalPort() const {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
        throw socketError("getsockname");
    }
    return ntohs(address.sin_port);
}

std::string TCPSocket::getLocalAddress() const {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
        throw socketError("getsockname");
    }
    char text[INET_ADDRSTRLEN] = {};
    if (!::inet_ntop(AF_INET, &address.sin_addr, text, sizeof(text))) {
        throw socketError("inet_ntop");
    }
    return text;
}

void TCPSocket::listen(int backlog) {
    if (::listen(sockfd, backlog) < 0) throw socketError("listen");
}

TCPSocket TCPSocket::acceptClient(sockaddr_in* client_addr) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    int accepted;
    do {
        accepted = ::accept(sockfd, reinterpret_cast<sockaddr*>(&address), &length);
    } while (accepted < 0 && errno == EINTR);
    if (accepted < 0) throw socketError("accept");
    if (client_addr) *client_addr = address;
    return TCPSocket(accepted);
}

void TCPSocket::connectTo(const std::string& ip, std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip.c_str(), &address.sin_addr) != 1) {
        throw std::runtime_error("Invalid IPv4 address: " + ip);
    }

    int result;
    do {
        result = ::connect(sockfd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    } while (result < 0 && errno == EINTR);
    if (result < 0) throw socketError("connect");
}

void TCPSocket::sendAll(const std::string& data) {
    sendAll(data.data(), data.size());
}

void TCPSocket::sendAll(const std::vector<std::uint8_t>& data) {
    sendAll(data.data(), data.size());
}

void TCPSocket::sendAll(const void* data, std::size_t size) {
    const char* bytes = static_cast<const char*>(data);
    std::size_t sent_total = 0;
    while (sent_total < size) {
        ssize_t sent;
        do {
            sent = ::send(sockfd, bytes + sent_total, size - sent_total, MSG_NOSIGNAL);
        } while (sent < 0 && errno == EINTR);
        if (sent < 0) throw socketError("send");
        if (sent == 0) throw std::runtime_error("send: connection closed");
        sent_total += static_cast<std::size_t>(sent);
    }
}

int TCPSocket::recvData(char* buffer, std::size_t max_size, int timeout_ms) {
    if (max_size == 0) return 0;
    if (timeout_ms >= 0) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(sockfd, &read_set);
        timeval timeout{};
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        int ready;
        do {
            fd_set current_set = read_set;
            timeval current_timeout = timeout;
            ready = ::select(sockfd + 1, &current_set, nullptr, nullptr, &current_timeout);
        } while (ready < 0 && errno == EINTR);
        if (ready == 0) return -1;
        if (ready < 0) throw socketError("select");
    }

    ssize_t received;
    do {
        received = ::recv(sockfd, buffer, max_size, 0);
    } while (received < 0 && errno == EINTR);
    if (received < 0) throw socketError("recv");
    return static_cast<int>(received);
}

bool TCPSocket::recvAll(char* buffer, std::size_t size) {
    std::size_t received_total = 0;
    while (received_total < size) {
        const int received = recvData(buffer + received_total, size - received_total);
        if (received <= 0) return false;
        received_total += static_cast<std::size_t>(received);
    }
    return true;
}
