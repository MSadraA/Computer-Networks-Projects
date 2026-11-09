#include "chat_client.hpp"

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace utchat {

ChatClient::ChatClient(const std::string& host, int port)
    : host_(host),
      port_(port),
      sock_fd_(-1),
      stdin_open_(true),
      stdin_eof_pending_shutdown_(false) {}

ChatClient::~ChatClient() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
    }
}

bool ChatClient::connect_to_server() {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port_);
    if (getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
        std::cerr << "Failed to resolve host: " << host_ << "\n";
        return false;
    }

    sock_fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_fd_ < 0) {
        perror("socket");
        freeaddrinfo(res);
        return false;
    }

    if (connect(sock_fd_, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sock_fd_);
        sock_fd_ = -1;
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    int flags = fcntl(sock_fd_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);
    }

    return true;
}

bool ChatClient::handle_socket_read() {
    char buf[4096];
    ssize_t n = recv(sock_fd_, buf, sizeof(buf), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return true;
        }
        return false;
    }
    if (n == 0) {
        if (!sock_in_buf_.empty()) {
            if (sock_in_buf_.back() == '\r') {
                sock_in_buf_.pop_back();
            }
            std::cout << sock_in_buf_ << "\n" << std::flush;
            sock_in_buf_.clear();
        }
        return false;
    }

    sock_in_buf_.append(buf, static_cast<size_t>(n));
    size_t pos = 0;
    while ((pos = sock_in_buf_.find('\n')) != std::string::npos) {
        std::string line = sock_in_buf_.substr(0, pos);
        sock_in_buf_.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::cout << line << "\n" << std::flush;
    }
    return true;
}

bool ChatClient::handle_socket_write() {
    if (!sock_out_buf_.empty()) {
        ssize_t sent = send(sock_fd_, sock_out_buf_.data(), sock_out_buf_.size(), MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return true;
            }
            return false;
        }
        sock_out_buf_.erase(0, static_cast<size_t>(sent));
    }

    if (sock_out_buf_.empty() && stdin_eof_pending_shutdown_) {
        shutdown(sock_fd_, SHUT_WR);
        stdin_eof_pending_shutdown_ = false;
    }
    return true;
}

void ChatClient::handle_stdin_read() {
    char buf[4096];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        stdin_open_ = false;
        stdin_eof_pending_shutdown_ = true;
        return;
    }
    if (n == 0) {
        if (!stdin_buf_.empty()) {
            if (stdin_buf_.back() == '\r') {
                stdin_buf_.pop_back();
            }
            stdin_buf_.push_back('\n');
            sock_out_buf_ += stdin_buf_;
            stdin_buf_.clear();
        }
        stdin_open_ = false;
        if (sock_out_buf_.empty()) {
            shutdown(sock_fd_, SHUT_WR);
        } else {
            stdin_eof_pending_shutdown_ = true;
        }
        return;
    }

    stdin_buf_.append(buf, static_cast<size_t>(n));
    size_t pos = 0;
    while ((pos = stdin_buf_.find('\n')) != std::string::npos) {
        std::string line = stdin_buf_.substr(0, pos);
        stdin_buf_.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        line.push_back('\n');
        sock_out_buf_ += line;
    }
}

void ChatClient::run() {
    while (true) {
        std::vector<pollfd> pfds;

        pollfd spfd{};
        spfd.fd = sock_fd_;
        spfd.events = POLLIN;
        if (!sock_out_buf_.empty() || stdin_eof_pending_shutdown_) {
            spfd.events |= POLLOUT;
        }
        pfds.push_back(spfd);

        if (stdin_open_) {
            pollfd ipfd{};
            ipfd.fd = STDIN_FILENO;
            ipfd.events = POLLIN;
            pfds.push_back(ipfd);
        }

        int ready = poll(pfds.data(), pfds.size(), -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        if (pfds[0].revents & (POLLIN | POLLERR | POLLHUP)) {
            if (!handle_socket_read()) {
                break;
            }
        }

        if (pfds[0].revents & POLLOUT) {
            if (!handle_socket_write()) {
                break;
            }
        }

        if (stdin_open_ && pfds.size() > 1 && (pfds[1].revents & (POLLIN | POLLERR | POLLHUP))) {
            handle_stdin_read();
        }
    }
}

} 

int main(int argc, char* argv[]) {
    std::signal(SIGPIPE, SIG_IGN);

    std::string host = "127.0.0.1";
    int port = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else {
            std::cerr << "Usage: " << argv[0] << " [--host <H>] --port <P>\n";
            return 1;
        }
    }

    if (port <= 0 || port > 65535) {
        std::cerr << "Usage: " << argv[0] << " [--host <H>] --port <P>\n";
        return 1;
    }

    utchat::ChatClient client(host, port);
    if (!client.connect_to_server()) {
        return 1;
    }
    client.run();
    return 0;
}