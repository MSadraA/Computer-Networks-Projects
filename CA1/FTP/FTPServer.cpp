#include "FTPServer.hpp"
#include "FTPParser.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

std::string joinPath(const std::string& directory, const std::string& filename) {
    return directory + "/" + filename;
}

}

FTPServer::FTPServer(const ServerConfig& cfg)
    : config(cfg), running(true) {
    struct stat info{};
    if (::stat(config.root_dir.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) {
        throw std::runtime_error("Root path is not an accessible directory: " + config.root_dir);
    }
    listen_socket.bind(config.port);
    listen_socket.listen(16);
}

void FTPServer::run() {
    std::cout << "FTP server listening on port " << config.port << '\n';
    while (running) {
        try {
            TCPSocket client = listen_socket.acceptClient();
            std::thread(&FTPServer::handleClient, this, std::move(client)).detach();
        } catch (const std::exception& error) {
            if (running) std::cerr << "Accept error: " << error.what() << '\n';
        }
    }
}

void FTPServer::handleClient(TCPSocket client_socket) {
    ClientSession session;
    try {
        client_socket.sendAll(FTPParser::formatReply(
            ftp_protocol::RESP_READY, "UT-FTP ready"));

        std::string line;
        bool discarding_line = false;
        char input[1024];

        while (true) {
            int count = client_socket.recvData(input, sizeof(input));
            if (count <= 0) break;

            for (int i = 0; i < count; ++i) {
                const char value = input[i];
                if (value == '\n') {
                    if (discarding_line) {
                        client_socket.sendAll(FTPParser::formatReply(
                            ftp_protocol::ERR_SYNTAX, "SYNTAX_ERROR_PARAMS"));
                    } else {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        if (!line.empty() && !processCommand(client_socket, session, line)) {
                            return;
                        }
                    }
                    line.clear();
                    discarding_line = false;
                    continue;
                }

                if (!discarding_line) {
                    line.push_back(value);
                    std::size_t effective_length = line.size();
                    if (effective_length > 0 && line.back() == '\r') --effective_length;
                    if (effective_length > ftp_protocol::MAX_COMMAND_LENGTH) {
                        line.clear();
                        discarding_line = true;
                    }
                }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Client session error: " << error.what() << '\n';
    }
}

bool FTPServer::processCommand(TCPSocket& client, ClientSession& session,
                               const std::string& line) {
    const ftp_protocol::ControlMessage message = FTPParser::parseCommand(line);
    using ftp_protocol::CommandType;

    if (message.type == CommandType::UNKNOWN) {
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_UNKNOWN, "UNKNOWN_COMMAND"));
        return true;
    }

    if (message.type == CommandType::QUIT) {
        if (!message.argument.empty()) {
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::ERR_SYNTAX, "SYNTAX_ERROR_PARAMS"));
            return true;
        }
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::RESP_GOODBYE, "Goodbye"));
        return false;
    }

    if (message.type == CommandType::USER) {
        if (message.argument.empty()) {
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::ERR_SYNTAX, "SYNTAX_ERROR_PARAMS"));
        } else if (session.logged_in) {
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::ERR_SEQUENCE, "BAD_SEQUENCE"));
        } else {
            session.username = message.argument;
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::RESP_PASS_REQ, "Password required"));
        }
        return true;
    }

    if (message.type == CommandType::PASS) {
        if (session.username.empty()) {
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::ERR_SEQUENCE, "BAD_SEQUENCE"));
        } else if (message.argument.empty()) {
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::ERR_SYNTAX, "SYNTAX_ERROR_PARAMS"));
        } else {
            session.logged_in = true;
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::RESP_LOGGED_IN, "Logged in"));
        }
        return true;
    }

    if (!session.logged_in &&
        message.type != CommandType::LIST &&
        message.type != CommandType::RETR &&
        message.type != CommandType::STOR) {
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_NOT_LOGGED_IN, "NOT_LOGGED_IN"));
        return true;
    }

    switch (message.type) {
        case CommandType::MODE:
            if (message.argument.empty()) {
                client.sendAll(FTPParser::formatReply(
                    ftp_protocol::ERR_SYNTAX, "SYNTAX_ERROR_PARAMS"));
            } else if (message.argument == "B") {
                session.mode_b_set = true;
                client.sendAll(FTPParser::formatReply(ftp_protocol::RESP_OK, "OK"));
            } else {
                client.sendAll(FTPParser::formatReply(
                    ftp_protocol::ERR_NO_IMPL, "COMMAND_NOT_IMPLEMENTED"));
            }
            break;

        case CommandType::PASV: {
            if (!message.argument.empty()) {
                client.sendAll(FTPParser::formatReply(
                    ftp_protocol::ERR_SYNTAX, "SYNTAX_ERROR_PARAMS"));
                break;
            }
            session.pasv_socket.reset(new TCPSocket());
            session.pasv_socket->bindToAny();
            session.pasv_socket->listen(1);

            const std::uint16_t port = session.pasv_socket->getLocalPort();
            std::string address = client.getLocalAddress();
            std::replace(address.begin(), address.end(), '.', ',');
            const std::string response = "Entering Passive Mode (" + address + "," +
                std::to_string(port / 256) + "," + std::to_string(port % 256) + ")";
            client.sendAll(FTPParser::formatReply(ftp_protocol::RESP_PASV, response));
            break;
        }

        case CommandType::LIST:
            if (!message.argument.empty()) {
                client.sendAll(FTPParser::formatReply(
                    ftp_protocol::ERR_SYNTAX, "SYNTAX_ERROR_PARAMS"));
            } else {
                handleLIST(client, session);
            }
            break;

        case CommandType::RETR:
            handleRETR(client, session, message.argument);
            break;

        case CommandType::STOR:
            handleSTOR(client, session, message.argument);
            break;

        default:
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::ERR_UNKNOWN, "UNKNOWN_COMMAND"));
            break;
    }
    return true;
}

bool FTPServer::checkTransferErrors(TCPSocket& client, ClientSession& session,
                                    const std::string& filename, bool needs_filename,
                                    bool check_existing_file) {
    if (!session.pasv_socket) {
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_NO_DATA_CONN, "CANT_OPEN_DATA_CONNECTION"));
        return false;
    }
    if (!session.mode_b_set) {
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_SEQUENCE, "BAD_SEQUENCE"));
        return false;
    }
    if (!filename.empty() && !FTPParser::isValidFileName(filename)) {
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_BAD_NAME, "NAME_NOT_ALLOWED"));
        return false;
    }
    if (needs_filename && filename.empty()) {
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_SYNTAX, "SYNTAX_ERROR_PARAMS"));
        return false;
    }
    if (!session.logged_in) {
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_NOT_LOGGED_IN, "NOT_LOGGED_IN"));
        return false;
    }
    if (check_existing_file) {
        struct stat info{};
        const std::string path = joinPath(config.root_dir, filename);
        if (::lstat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::ERR_NOT_FOUND, "FILE_NOT_FOUND"));
            return false;
        }
    }
    return true;
}

void FTPServer::sendModeBData(TCPSocket& data_socket,
                              const std::vector<std::uint8_t>& data, bool is_eof) {
    if (data.size() > 65535) throw std::runtime_error("MODE B block is too large");
    const std::uint8_t descriptor = is_eof
        ? ftp_protocol::DESC_EOF : ftp_protocol::DESC_DATA;
    data_socket.sendAll(FTPParser::createModeBHeader(
        descriptor, static_cast<std::uint16_t>(data.size())));
    if (!data.empty()) data_socket.sendAll(data);
}

void FTPServer::handleLIST(TCPSocket& client, ClientSession& session) {
    if (!checkTransferErrors(client, session, "", false, false)) return;

    DIR* directory = ::opendir(config.root_dir.c_str());
    if (!directory) {
        session.pasv_socket.reset();
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_NOT_FOUND, "FILE_NOT_FOUND"));
        return;
    }

    std::vector<std::pair<std::string, off_t> > entries;
    while (dirent* entry = ::readdir(directory)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." ||
            name.compare(0, 12, ".ftp-upload-") == 0 ||
            !FTPParser::isValidFileName(name)) continue;

        struct stat info{};
        if (::lstat(joinPath(config.root_dir, name).c_str(), &info) == 0 &&
            S_ISREG(info.st_mode)) {
            entries.push_back(std::make_pair(name, info.st_size));
        }
    }
    ::closedir(directory);
    std::sort(entries.begin(), entries.end(),
        [](const std::pair<std::string, off_t>& left,
           const std::pair<std::string, off_t>& right) {
            return left.first < right.first;
        });

    std::string listing;
    for (const auto& entry : entries) {
        listing += entry.first + " " + std::to_string(entry.second) + "\n";
    }

    client.sendAll(FTPParser::formatReply(
        ftp_protocol::RESP_DATA_OPEN, "Opening data connection"));
    TCPSocket data_socket = session.pasv_socket->acceptClient();
    session.pasv_socket.reset();

    try {
        std::size_t offset = 0;
        while (offset < listing.size()) {
            const std::size_t amount = std::min(
                ftp_protocol::MAX_BLOCK_DATA, listing.size() - offset);
            std::vector<std::uint8_t> block(listing.begin() + offset,
                                             listing.begin() + offset + amount);
            sendModeBData(data_socket, block, offset + amount == listing.size());
            offset += amount;
        }
        if (listing.empty()) sendModeBData(data_socket, {}, true);
        data_socket.close();
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::RESP_DATA_DONE, "Transfer complete"));
    } catch (const std::exception&) {
        data_socket.close();
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_CONN_CLOSED, "CONNECTION_CLOSED"));
    }
}

void FTPServer::handleRETR(TCPSocket& client, ClientSession& session,
                           const std::string& filename) {
    if (!checkTransferErrors(client, session, filename, true, true)) return;

    std::ifstream file(joinPath(config.root_dir, filename), std::ios::binary);
    if (!file) {
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_NOT_FOUND, "FILE_NOT_FOUND"));
        return;
    }

    client.sendAll(FTPParser::formatReply(
        ftp_protocol::RESP_DATA_OPEN, "Opening data connection"));
    TCPSocket data_socket = session.pasv_socket->acceptClient();
    session.pasv_socket.reset();

    try {
        std::vector<std::uint8_t> buffer(ftp_protocol::MAX_BLOCK_DATA);
        while (true) {
            file.read(reinterpret_cast<char*>(buffer.data()),
                      static_cast<std::streamsize>(buffer.size()));
            const std::streamsize count = file.gcount();
            if (count < 0 || file.bad()) throw std::runtime_error("File read failed");

            if (count == static_cast<std::streamsize>(buffer.size())) {
                if (file.peek() == std::ifstream::traits_type::eof()) {
                    sendModeBData(data_socket, buffer, true);
                    break;
                }
                sendModeBData(data_socket, buffer, false);
                continue;
            }

            std::vector<std::uint8_t> final_block(
                buffer.begin(), buffer.begin() + static_cast<std::size_t>(count));
            sendModeBData(data_socket, final_block, true);
            break;
        }
        data_socket.close();
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::RESP_DATA_DONE, "Transfer complete"));
    } catch (const std::exception&) {
        data_socket.close();
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_CONN_CLOSED, "CONNECTION_CLOSED"));
    }
}

void FTPServer::handleSTOR(TCPSocket& client, ClientSession& session,
                           const std::string& filename) {
    if (!checkTransferErrors(client, session, filename, true, false)) return;

    const std::string target_path = joinPath(config.root_dir, filename);
    std::string temporary_path = joinPath(config.root_dir, ".ftp-upload-XXXXXX");
    std::vector<char> temporary_name(temporary_path.begin(), temporary_path.end());
    temporary_name.push_back('\0');
    const int temporary_fd = ::mkstemp(temporary_name.data());
    if (temporary_fd < 0) {
        session.pasv_socket.reset();
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_CONN_CLOSED, "CONNECTION_CLOSED"));
        return;
    }
    ::close(temporary_fd);
    temporary_path = temporary_name.data();

    client.sendAll(FTPParser::formatReply(
        ftp_protocol::RESP_DATA_OPEN, "Opening data connection"));
    TCPSocket data_socket = [&]() -> TCPSocket {
        try {
            return session.pasv_socket->acceptClient();
        } catch (...) {
            ::remove(temporary_path.c_str());
            client.sendAll(FTPParser::formatReply(
                ftp_protocol::ERR_CONN_CLOSED, "CONNECTION_CLOSED"));
            throw;
        }
    }();
    session.pasv_socket.reset();

    bool transfer_ok = false;
    try {
        std::ofstream file(temporary_path.c_str(),
                           std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("Cannot open temporary file");

        bool eof_received = false;
        while (!eof_received) {
            std::uint8_t header[3];
            if (!data_socket.recvAll(reinterpret_cast<char*>(header), sizeof(header))) {
                throw std::runtime_error("Incomplete MODE B header");
            }

            const std::uint8_t descriptor = header[0];
            const std::uint16_t length = static_cast<std::uint16_t>(
                (static_cast<unsigned int>(header[1]) << 8) | header[2]);
            if (descriptor != ftp_protocol::DESC_DATA &&
                descriptor != ftp_protocol::DESC_EOF) {
                throw std::runtime_error("Invalid MODE B descriptor");
            }

            std::vector<char> block(length);
            if (length > 0 &&
                !data_socket.recvAll(block.data(), block.size())) {
                throw std::runtime_error("Incomplete MODE B payload");
            }

            if (!block.empty()) {
                file.write(block.data(), static_cast<std::streamsize>(block.size()));
                if (!file) throw std::runtime_error("File write failed");
            }
            eof_received = descriptor == ftp_protocol::DESC_EOF;
        }

        file.flush();
        if (!file) throw std::runtime_error("File flush failed");
        file.close();
        if (!file) throw std::runtime_error("File close failed");

        if (::rename(temporary_path.c_str(), target_path.c_str()) != 0) {
            throw std::runtime_error("Atomic file replacement failed");
        }
        transfer_ok = true;
    } catch (const std::exception&) {
        transfer_ok = false;
    }

    data_socket.close();
    if (!transfer_ok) {
        ::remove(temporary_path.c_str());
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::ERR_CONN_CLOSED, "CONNECTION_CLOSED"));
    } else {
        client.sendAll(FTPParser::formatReply(
            ftp_protocol::RESP_DATA_DONE, "Transfer complete"));
    }
}
