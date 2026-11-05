#include "UDPSocket.hpp"

struct ServerConfig {
    uint16_t port;
    float loss_probability; 
    unsigned int seed;      
};

class PingServer {
public:
    PingServer(const ServerConfig& config);
    
    void run();

private:
    ServerConfig config;
    UDPSocket socket;

    bool shouldDrop() const; 
};