#ifndef CHAT_SERVER_HPP
#define CHAT_SERVER_HPP

#include "chat_protocol.hpp"
#include <string>
#include <map>
#include <set>
#include <vector>
#include <cstdint>

namespace utchat {

struct Room {
    std::string name;
    uint64_t next_msg_id = 1;
    std::set<int> member_fds;
};

struct ClientSession {
    int fd = -1;
    bool registered = false;
    std::string nick;
    std::string in_buf;
    bool line_overflow = false;
    std::string out_buf;
    bool closing = false;
    std::set<std::string> joined_rooms;
};

class ChatServer {
public:
    explicit ChatServer(int port);
    ~ChatServer();

    bool init();
    void run();

private:
    int port_;
    int listen_fd_;
    std::map<int, ClientSession> clients_;
    std::map<std::string, int> nick_to_fd_;
    std::map<std::string, Room> rooms_;

    void accept_new_client();
    void handle_client_read(int fd);
    void handle_client_write(int fd);
    void disconnect_client(int fd);

    void process_line(int fd, const std::string& line);
    void queue_raw(int fd, const std::string& data);
    void queue_line(int fd, const std::string& line);

    // Command handlers
    void cmd_ping(int fd, bool has_space);
    void cmd_quit(int fd, ClientSession& client, bool has_space);
    void cmd_nick(int fd, ClientSession& client, bool has_space, const std::string& rest);
    void cmd_join(int fd, ClientSession& client, bool has_space, const std::string& rest);
    void cmd_part(int fd, ClientSession& client, bool has_space, const std::string& rest);
    void cmd_msg(int fd, ClientSession& client, bool has_space, const std::string& rest);
    void cmd_privmsg(int fd, ClientSession& client, bool has_space, const std::string& rest);
    void cmd_list(int fd, bool has_space);
    void cmd_who(int fd, bool has_space, const std::string& rest);
};

} 

#endif