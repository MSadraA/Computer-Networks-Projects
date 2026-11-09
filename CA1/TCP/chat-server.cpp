#include "chat_server.hpp"

#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace utchat {

ChatServer::ChatServer(int port) : port_(port), listen_fd_(-1) {}

ChatServer::~ChatServer() {
    for (auto& kv : clients_) {
        if (kv.first >= 0) {
            close(kv.first);
        }
    }
    if (listen_fd_ >= 0) {
        close(listen_fd_);
    }
}

bool ChatServer::init() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        perror("socket");
        return false;
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        return false;
    }

    int flags = fcntl(listen_fd_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        return false;
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        perror("listen");
        return false;
    }

    return true;
}

void ChatServer::queue_raw(int fd, const std::string& data) {
    auto it = clients_.find(fd);
    if (it == clients_.end() || it->second.closing) {
        return;
    }
    it->second.out_buf += data;
}

void ChatServer::queue_line(int fd, const std::string& line) {
    queue_raw(fd, line + "\n");
}

void ChatServer::accept_new_client() {
    while (true) {
        sockaddr_in cli_addr{};
        socklen_t cli_len = sizeof(cli_addr);
        int cfd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli_addr), &cli_len);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        int flags = fcntl(cfd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(cfd, F_SETFL, flags | O_NONBLOCK);
        }

        ClientSession session;
        session.fd = cfd;
        clients_[cfd] = session;
        queue_line(cfd, WELCOME_BANNER);
    }
}

void ChatServer::disconnect_client(int fd) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) {
        return;
    }

    ClientSession session = it->second;
    clients_.erase(it);

    if (session.registered) {
        nick_to_fd_.erase(session.nick);
        for (const std::string& rname : session.joined_rooms) {
            auto rit = rooms_.find(rname);
            if (rit != rooms_.end()) {
                rit->second.member_fds.erase(fd);
                std::string ev = "EVENT LEAVE " + rname + " " + session.nick + "\n";
                for (int member_fd : rit->second.member_fds) {
                    queue_raw(member_fd, ev);
                }
            }
        }
    }

    close(fd);
}

void ChatServer::handle_client_read(int fd) {
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            return;
        }
        disconnect_client(fd);
        return;
    }

    auto cit = clients_.find(fd);
    if (cit == clients_.end()) {
        return;
    }

    for (ssize_t i = 0; i < n; ++i) {
        if (cit->second.closing) {
            break;
        }
        char c = buf[i];
        if (c == '\n') {
            if (cit->second.line_overflow) {
                cit->second.in_buf.clear();
                cit->second.line_overflow = false;
                queue_line(fd, "ERR 413 LINE_TOO_LONG");
            } else {
                std::string line = cit->second.in_buf;
                cit->second.in_buf.clear();
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (line.size() > MAX_LINE_LENGTH) {
                    queue_line(fd, "ERR 413 LINE_TOO_LONG");
                } else {
                    process_line(fd, line);
                }
            }
        } else {
            if (!cit->second.line_overflow) {
                if (cit->second.in_buf.size() == MAX_LINE_LENGTH + 1) {
                    cit->second.line_overflow = true;
                    cit->second.in_buf.clear();
                } else {
                    cit->second.in_buf.push_back(c);
                    if (cit->second.in_buf.size() == MAX_LINE_LENGTH + 1 && c != '\r') {
                        cit->second.line_overflow = true;
                        cit->second.in_buf.clear();
                    }
                }
            }
        }
    }
}

void ChatServer::handle_client_write(int fd) {
    auto cit = clients_.find(fd);
    if (cit == clients_.end()) {
        return;
    }
    ClientSession& client = cit->second;

    if (!client.out_buf.empty()) {
        ssize_t sent = send(fd, client.out_buf.data(), client.out_buf.size(), MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                disconnect_client(fd);
                return;
            }
        } else if (sent > 0) {
            client.out_buf.erase(0, static_cast<size_t>(sent));
        }
    }

    if (client.out_buf.empty() && client.closing) {
        close(fd);
        clients_.erase(cit);
    }
}

void ChatServer::process_line(int fd, const std::string& line) {
    if (line.empty()) {
        return;
    }

    auto cit = clients_.find(fd);
    if (cit == clients_.end() || cit->second.closing) {
        return;
    }
    ClientSession& client = cit->second;

    size_t sp = line.find(' ');
    std::string cmd = (sp == std::string::npos) ? line : line.substr(0, sp);
    std::string rest = (sp == std::string::npos) ? "" : line.substr(sp + 1);
    bool has_space = (sp != std::string::npos);

    if (!is_known_command(cmd)) {
        queue_line(fd, "ERR 400 UNKNOWN_COMMAND");
        return;
    }

    if (!client.registered && cmd != "NICK" && cmd != "PING" && cmd != "QUIT") {
        queue_line(fd, "ERR 401 NOT_REGISTERED");
        return;
    }

    if (cmd == "PING") {
        cmd_ping(fd, has_space);
    } else if (cmd == "QUIT") {
        cmd_quit(fd, client, has_space);
    } else if (cmd == "NICK") {
        cmd_nick(fd, client, has_space, rest);
    } else if (cmd == "JOIN") {
        cmd_join(fd, client, has_space, rest);
    } else if (cmd == "PART") {
        cmd_part(fd, client, has_space, rest);
    } else if (cmd == "MSG") {
        cmd_msg(fd, client, has_space, rest);
    } else if (cmd == "PRIVMSG") {
        cmd_privmsg(fd, client, has_space, rest);
    } else if (cmd == "LIST") {
        cmd_list(fd, has_space);
    } else if (cmd == "WHO") {
        cmd_who(fd, has_space, rest);
    }
}

void ChatServer::cmd_ping(int fd, bool has_space) {
    if (has_space) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    queue_line(fd, "PONG");
}

void ChatServer::cmd_quit(int fd, ClientSession& client, bool has_space) {
    if (has_space) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    queue_line(fd, "OK BYE");
    client.closing = true;

    if (client.registered) {
        nick_to_fd_.erase(client.nick);
        for (const std::string& rname : client.joined_rooms) {
            auto rit = rooms_.find(rname);
            if (rit != rooms_.end()) {
                rit->second.member_fds.erase(fd);
                std::string ev = "EVENT LEAVE " + rname + " " + client.nick + "\n";
                for (int member_fd : rit->second.member_fds) {
                    queue_raw(member_fd, ev);
                }
            }
        }
        client.joined_rooms.clear();
        client.registered = false;
    }
}

void ChatServer::cmd_nick(int fd, ClientSession& client, bool has_space, const std::string& rest) {
    if (!has_space || !is_valid_nick(rest)) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    if (client.registered) {
        queue_line(fd, "ERR 409 ALREADY_REGISTERED");
        return;
    }
    if (nick_to_fd_.find(rest) != nick_to_fd_.end()) {
        queue_line(fd, "ERR 409 NICK_IN_USE");
        return;
    }

    client.registered = true;
    client.nick = rest;
    nick_to_fd_[rest] = fd;
    queue_line(fd, "OK NICK " + rest);
}

void ChatServer::cmd_join(int fd, ClientSession& client, bool has_space, const std::string& rest) {
    if (!has_space || !is_valid_room(rest)) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    if (client.joined_rooms.find(rest) != client.joined_rooms.end()) {
        queue_line(fd, "ERR 409 ALREADY_IN_ROOM");
        return;
    }

    Room& room = rooms_[rest];
    if (room.name.empty()) {
        room.name = rest;
    }
    client.joined_rooms.insert(rest);
    room.member_fds.insert(fd);

    queue_line(fd, "OK JOIN " + rest);
    std::string ev = "EVENT JOIN " + rest + " " + client.nick + "\n";
    for (int member_fd : room.member_fds) {
        if (member_fd != fd) {
            queue_raw(member_fd, ev);
        }
    }
}

void ChatServer::cmd_part(int fd, ClientSession& client, bool has_space, const std::string& rest) {
    if (!has_space || !is_valid_room(rest)) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    if (client.joined_rooms.find(rest) == client.joined_rooms.end()) {
        queue_line(fd, "ERR 403 NOT_IN_ROOM");
        return;
    }

    client.joined_rooms.erase(rest);
    auto rit = rooms_.find(rest);
    queue_line(fd, "OK PART " + rest);

    if (rit != rooms_.end()) {
        rit->second.member_fds.erase(fd);
        std::string ev = "EVENT LEAVE " + rest + " " + client.nick + "\n";
        for (int member_fd : rit->second.member_fds) {
            queue_raw(member_fd, ev);
        }
    }
}

void ChatServer::cmd_msg(int fd, ClientSession& client, bool has_space, const std::string& rest) {
    if (!has_space) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    size_t sp2 = rest.find(' ');
    if (sp2 == std::string::npos) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    std::string rname = rest.substr(0, sp2);
    std::string text = rest.substr(sp2 + 1);
    if (!is_valid_room(rname) || text.empty()) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    if (client.joined_rooms.find(rname) == client.joined_rooms.end()) {
        queue_line(fd, "ERR 403 NOT_IN_ROOM");
        return;
    }

    Room& room = rooms_[rname];
    uint64_t msg_id = room.next_msg_id++;
    queue_line(fd, "OK MSG " + rname + " " + std::to_string(msg_id));

    std::string broadcast = "FROM " + rname + " " + client.nick + " " +
                            std::to_string(msg_id) + " " + text + "\n";
    for (int member_fd : room.member_fds) {
        if (member_fd != fd) {
            queue_raw(member_fd, broadcast);
        }
    }
}

void ChatServer::cmd_privmsg(int fd, ClientSession& client, bool has_space, const std::string& rest) {
    if (!has_space) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    size_t sp2 = rest.find(' ');
    if (sp2 == std::string::npos) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    std::string target_nick = rest.substr(0, sp2);
    std::string text = rest.substr(sp2 + 1);
    if (!is_valid_nick(target_nick) || text.empty()) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }

    auto nit = nick_to_fd_.find(target_nick);
    if (nit == nick_to_fd_.end()) {
        queue_line(fd, "ERR 404 NO_SUCH_NICK");
        return;
    }

    int target_fd = nit->second;
    queue_line(fd, "OK PRIVMSG " + target_nick);
    queue_line(target_fd, "PRIV " + client.nick + " " + text);
}

void ChatServer::cmd_list(int fd, bool has_space) {
    if (has_space) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    std::string resp = "OK LIST " + std::to_string(rooms_.size()) + "\n";
    for (const auto& kv : rooms_) {
        resp += "ROOM " + kv.first + " " + std::to_string(kv.second.member_fds.size()) + "\n";
    }
    queue_raw(fd, resp);
}

void ChatServer::cmd_who(int fd, bool has_space, const std::string& rest) {
    if (!has_space || !is_valid_room(rest)) {
        queue_line(fd, "ERR 400 BAD_SYNTAX");
        return;
    }
    auto rit = rooms_.find(rest);
    if (rit == rooms_.end()) {
        queue_line(fd, "ERR 404 NO_SUCH_ROOM");
        return;
    }

    std::vector<std::string> nicks;
    nicks.reserve(rit->second.member_fds.size());
    for (int member_fd : rit->second.member_fds) {
        auto mcit = clients_.find(member_fd);
        if (mcit != clients_.end() && mcit->second.registered) {
            nicks.push_back(mcit->second.nick);
        }
    }
    std::sort(nicks.begin(), nicks.end());

    std::string resp = "OK WHO " + rest + " " + std::to_string(nicks.size()) + "\n";
    for (const std::string& n : nicks) {
        resp += "USER " + n + "\n";
    }
    queue_raw(fd, resp);
}

void ChatServer::run() {
    while (true) {
        std::vector<pollfd> pfds;
        pfds.reserve(clients_.size() + 1);

        pollfd lpfd{};
        lpfd.fd = listen_fd_;
        lpfd.events = POLLIN;
        pfds.push_back(lpfd);

        for (const auto& kv : clients_) {
            pollfd cpfd{};
            cpfd.fd = kv.first;
            cpfd.events = 0;
            if (!kv.second.closing) {
                cpfd.events |= POLLIN;
            }
            if (!kv.second.out_buf.empty() || kv.second.closing) {
                cpfd.events |= POLLOUT;
            }
            pfds.push_back(cpfd);
        }

        int ready = poll(pfds.data(), pfds.size(), -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        if (pfds[0].revents & POLLIN) {
            accept_new_client();
        }

        for (size_t i = 1; i < pfds.size(); ++i) {
            int cfd = pfds[i].fd;
            short rev = pfds[i].revents;

            if (rev & (POLLERR | POLLHUP | POLLNVAL)) {
                if (rev & POLLIN) {
                    handle_client_read(cfd);
                } else {
                    disconnect_client(cfd);
                }
                continue;
            }

            if (rev & POLLIN) {
                handle_client_read(cfd);
            }
            if (clients_.find(cfd) != clients_.end() && (rev & POLLOUT)) {
                handle_client_write(cfd);
            }
        }
    }
}

} 

int main(int argc, char* argv[]) {
    std::signal(SIGPIPE, SIG_IGN);

    int port = -1;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else {
            std::cerr << "Usage: " << argv[0] << " --port <P>\n";
            return 1;
        }
    }

    if (port <= 0 || port > 65535) {
        std::cerr << "Usage: " << argv[0] << " --port <P>\n";
        return 1;
    }

    utchat::ChatServer server(port);
    if (!server.init()) {
        return 1;
    }
    server.run();
    return 0;
}