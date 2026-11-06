#ifndef FTPSERVER_HPP
#define FTPSERVER_HPP

#include "TCPSocket.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct ServerConfig {
    std::uint16_t port;
    std::string root_dir;
};

class FTPServer {
public:
    explicit FTPServer(const ServerConfig& config);
    void run();

private:
    ServerConfig config;
    TCPSocket listen_socket;
    std::atomic<bool> running;

    struct ClientSession {
        bool logged_in = false;
        bool mode_b_set = false;
        std::string username;
        std::unique_ptr<TCPSocket> pasv_socket;
    };

    void handleClient(TCPSocket client_socket);
    bool processCommand(TCPSocket& client_socket, ClientSession& session,
                        const std::string& line);
    void handleLIST(TCPSocket& client_socket, ClientSession& session);
    void handleRETR(TCPSocket& client_socket, ClientSession& session,
                    const std::string& filename);
    void handleSTOR(TCPSocket& client_socket, ClientSession& session,
                    const std::string& filename);
    bool checkTransferErrors(TCPSocket& client_socket, ClientSession& session,
                             const std::string& filename, bool needs_filename,
                             bool check_existing_file);
    void sendModeBData(TCPSocket& data_socket,
                       const std::vector<std::uint8_t>& data, bool is_eof);
};

#endif
