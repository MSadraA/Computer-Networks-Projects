#ifndef FTPCLIENT_HPP
#define FTPCLIENT_HPP

#include "TCPSocket.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct ClientConfig {
    std::string host;
    std::uint16_t port;
};

class FTPClient {
public:
    explicit FTPClient(const ClientConfig& config);
    void run();

private:
    ClientConfig config;
    TCPSocket control_socket;
    std::unique_ptr<TCPSocket> data_socket;
    std::string control_buffer;

    std::string readControlLine();
    std::string sendCommand(const std::string& command);
    void handlePASV(const std::string& reply);
    void handleLIST();
    void handleRETR(const std::string& filename);
    void handleSTOR(const std::string& filename);
    bool readModeBBlock(std::vector<std::uint8_t>& data, bool& is_eof);
    void sendModeBData(const std::vector<std::uint8_t>& data, bool is_eof);
};

#endif
