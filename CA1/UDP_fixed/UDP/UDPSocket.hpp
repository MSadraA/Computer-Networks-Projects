#ifndef UDPSOCKET_HPP
#define UDPSOCKET_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class UDPSocket {
public:
    UDPSocket();
    ~UDPSocket();

    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;

    void bind(std::uint16_t port);
    void sendTo(const std::string& ip, std::uint16_t port, const std::string& data);

    // Returns the received byte count, -1 on timeout, and throws on socket errors.
    int recvFrom(char* buffer, std::size_t max_size, sockaddr_in& sender_addr,
                 int timeout_ms = 0);

private:
    int sockfd;
};

#endif
