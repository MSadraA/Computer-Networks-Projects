#ifndef TCPSOCKET_HPP
#define TCPSOCKET_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>

class TCPSocket {
public:
    TCPSocket();
    ~TCPSocket();

    TCPSocket(const TCPSocket&) = delete;
    TCPSocket& operator=(const TCPSocket&) = delete;

    TCPSocket(TCPSocket&& other) noexcept;
    TCPSocket& operator=(TCPSocket&& other) noexcept;

    void bind(std::uint16_t port);
    void bindToAny();
    void listen(int backlog = 5);
    TCPSocket acceptClient(sockaddr_in* client_addr = nullptr);
    std::uint16_t getLocalPort() const;
    std::string getLocalAddress() const;

    void connectTo(const std::string& ip, std::uint16_t port);

    void sendAll(const std::string& data);
    void sendAll(const std::vector<std::uint8_t>& data);
    int recvData(char* buffer, std::size_t max_size, int timeout_ms = -1);
    bool recvAll(char* buffer, std::size_t size);

    int getFd() const { return sockfd; }
    void close() noexcept;

private:
    explicit TCPSocket(int existing_fd);
    void sendAll(const void* data, std::size_t size);
    int sockfd;
};

#endif
