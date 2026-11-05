#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class UDPSocket {
public:
    UDPSocket();
    ~UDPSocket();

    void bind(uint16_t port);

    void sendTo(const std::string& ip, uint16_t port, const std::string& data);

    int recvFrom(char* buffer, size_t max_size, sockaddr_in& sender_addr, int timeout_ms = 0);

private:
    int sockfd;
};