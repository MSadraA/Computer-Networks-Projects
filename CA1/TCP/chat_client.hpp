#ifndef CHAT_CLIENT_HPP
#define CHAT_CLIENT_HPP

#include "chat_protocol.hpp"
#include <string>

namespace utchat {

class ChatClient {
public:
    ChatClient(const std::string& host, int port);
    ~ChatClient();

    bool connect_to_server();
    void run();

private:
    std::string host_;
    int port_;
    int sock_fd_;
    bool stdin_open_;
    bool stdin_eof_pending_shutdown_;
    std::string sock_in_buf_;
    std::string sock_out_buf_;
    std::string stdin_buf_;

    bool handle_socket_read();
    bool handle_socket_write();
    void handle_stdin_read();
};

} 

#endif 