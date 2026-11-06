#include "FTPClient.hpp"
#include "FTPParser.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace {

int responseCode(const std::string& reply) {
    if (reply.size() < 3) return 0;
    if (reply[0] < '0' || reply[0] > '9' ||
        reply[1] < '0' || reply[1] > '9' ||
        reply[2] < '0' || reply[2] > '9') return 0;
    return (reply[0] - '0') * 100 + (reply[1] - '0') * 10 + reply[2] - '0';
}

}

FTPClient::FTPClient(const ClientConfig& cfg)
    : config(cfg), control_socket() {
    control_socket.connectTo(config.host, config.port);
}

std::string FTPClient::readControlLine() {
    while (true) {
        const std::string::size_type newline = control_buffer.find('\n');
        if (newline != std::string::npos) {
            std::string line = control_buffer.substr(0, newline);
            control_buffer.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return line;
        }

        char input[1024];
        int count = control_socket.recvData(input, sizeof(input));
        if (count <= 0) {
            if (control_buffer.empty()) return "";
            throw std::runtime_error("Server closed the control connection mid-reply");
        }
        control_buffer.append(input, static_cast<std::size_t>(count));
        if (control_buffer.size() > 65536) {
            throw std::runtime_error("Control reply is too large");
        }
    }
}

std::string FTPClient::sendCommand(const std::string& command) {
    control_socket.sendAll(command + "\r\n");
    const std::string reply = readControlLine();
    if (!reply.empty()) std::cout << reply << '\n';
    return reply;
}

void FTPClient::run() {
    const std::string greeting = readControlLine();
    if (greeting.empty()) throw std::runtime_error("No greeting received from server");
    std::cout << greeting << '\n';
    if (responseCode(greeting) != ftp_protocol::RESP_READY) {
        throw std::runtime_error("Unexpected server greeting");
    }

    std::string command_line;
    while (std::getline(std::cin, command_line)) {
        if (command_line.empty()) continue;

        const ftp_protocol::ControlMessage message =
            FTPParser::parseCommand(command_line);
        const std::string reply = sendCommand(command_line);
        if (reply.empty()) break;

        switch (message.type) {
            case ftp_protocol::CommandType::PASV:
                if (responseCode(reply) == ftp_protocol::RESP_PASV) {
                    handlePASV(reply);
                }
                break;

            case ftp_protocol::CommandType::LIST:
                if (responseCode(reply) == ftp_protocol::RESP_DATA_OPEN) {
                    handleLIST();
                    std::string completion = readControlLine();
                    if (!completion.empty()) std::cout << completion << '\n';
                }
                break;

            case ftp_protocol::CommandType::RETR:
                if (responseCode(reply) == ftp_protocol::RESP_DATA_OPEN) {
                    handleRETR(message.argument);
                    std::string completion = readControlLine();
                    if (!completion.empty()) std::cout << completion << '\n';
                }
                break;

            case ftp_protocol::CommandType::STOR:
                if (responseCode(reply) == ftp_protocol::RESP_DATA_OPEN) {
                    handleSTOR(message.argument);
                    std::string completion = readControlLine();
                    if (!completion.empty()) std::cout << completion << '\n';
                }
                break;

            case ftp_protocol::CommandType::QUIT:
                return;

            default:
                break;
        }
    }
}

void FTPClient::handlePASV(const std::string& reply) {
    const std::string::size_type open = reply.find('(');
    const std::string::size_type close = reply.find(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        throw std::runtime_error("Malformed PASV response");
    }

    std::string tuple = reply.substr(open + 1, close - open - 1);
    for (char& value : tuple) {
        if (value == ',') value = ' ';
    }

    std::istringstream parser(tuple);
    int values[6];
    for (int i = 0; i < 6; ++i) {
        if (!(parser >> values[i]) || values[i] < 0 || values[i] > 255) {
            throw std::runtime_error("Invalid PASV address");
        }
    }
    std::string extra;
    if (parser >> extra) throw std::runtime_error("Invalid PASV response");

    const unsigned int port =
        static_cast<unsigned int>(values[4] * 256 + values[5]);
    if (port == 0 || port > 65535) throw std::runtime_error("Invalid PASV port");

    data_socket.reset(new TCPSocket());
    data_socket->connectTo(config.host, static_cast<std::uint16_t>(port));
}

bool FTPClient::readModeBBlock(std::vector<std::uint8_t>& data, bool& is_eof) {
    if (!data_socket) return false;

    std::uint8_t header[3];
    if (!data_socket->recvAll(reinterpret_cast<char*>(header), sizeof(header))) {
        return false;
    }

    if (header[0] != ftp_protocol::DESC_DATA &&
        header[0] != ftp_protocol::DESC_EOF) {
        return false;
    }

    const std::uint16_t length = static_cast<std::uint16_t>(
        (static_cast<unsigned int>(header[1]) << 8) | header[2]);
    data.resize(length);
    if (length > 0 &&
        !data_socket->recvAll(reinterpret_cast<char*>(data.data()), length)) {
        return false;
    }

    is_eof = header[0] == ftp_protocol::DESC_EOF;
    return true;
}

void FTPClient::sendModeBData(const std::vector<std::uint8_t>& data, bool is_eof) {
    if (!data_socket) throw std::runtime_error("Data connection is not open");
    if (data.size() > 65535) throw std::runtime_error("MODE B block is too large");

    const std::uint8_t descriptor = is_eof
        ? ftp_protocol::DESC_EOF : ftp_protocol::DESC_DATA;
    data_socket->sendAll(FTPParser::createModeBHeader(
        descriptor, static_cast<std::uint16_t>(data.size())));
    if (!data.empty()) data_socket->sendAll(data);
}

void FTPClient::handleLIST() {
    if (!data_socket) throw std::runtime_error("PASV must be used before LIST");

    bool eof = false;
    while (!eof) {
        std::vector<std::uint8_t> block;
        if (!readModeBBlock(block, eof)) {
            data_socket.reset();
            throw std::runtime_error("Invalid or incomplete MODE B listing");
        }
        if (!block.empty()) {
            std::cout.write(reinterpret_cast<const char*>(block.data()),
                            static_cast<std::streamsize>(block.size()));
        }
    }
    std::cout.flush();
    data_socket.reset();
}

void FTPClient::handleRETR(const std::string& filename) {
    if (!FTPParser::isValidFileName(filename)) {
        if (data_socket) data_socket.reset();
        throw std::runtime_error("Invalid local file name");
    }
    if (!data_socket) throw std::runtime_error("PASV must be used before RETR");

    std::string temporary = filename + ".part." + std::to_string(::getpid());
    std::ofstream file(temporary.c_str(), std::ios::binary | std::ios::trunc);
    if (!file) {
        data_socket.reset();
        throw std::runtime_error("Cannot create local destination file");
    }

    bool eof = false;
    bool success = true;
    while (!eof) {
        std::vector<std::uint8_t> block;
        if (!readModeBBlock(block, eof)) {
            success = false;
            break;
        }
        if (!block.empty()) {
            file.write(reinterpret_cast<const char*>(block.data()),
                       static_cast<std::streamsize>(block.size()));
            if (!file) {
                success = false;
                break;
            }
        }
    }

    file.close();
    data_socket.reset();
    if (!file) success = false;

    if (success) {
        if (std::rename(temporary.c_str(), filename.c_str()) != 0) success = false;
    }
    if (!success) {
        std::remove(temporary.c_str());
        throw std::runtime_error("File download failed");
    }
}

void FTPClient::handleSTOR(const std::string& filename) {
    if (!FTPParser::isValidFileName(filename)) {
        if (data_socket) data_socket.reset();
        throw std::runtime_error("Invalid local file name");
    }
    if (!data_socket) throw std::runtime_error("PASV must be used before STOR");

    std::ifstream file(filename.c_str(), std::ios::binary);
    if (!file) {
        data_socket.reset();
        return;
    }

    std::vector<std::uint8_t> buffer(ftp_protocol::MAX_BLOCK_DATA);
    while (true) {
        file.read(reinterpret_cast<char*>(buffer.data()),
                  static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = file.gcount();
        if (count < 0 || file.bad()) {
            data_socket.reset();
            throw std::runtime_error("Local file read failed");
        }

        if (count == static_cast<std::streamsize>(buffer.size())) {
            if (file.peek() == std::ifstream::traits_type::eof()) {
                sendModeBData(buffer, true);
                break;
            }
            sendModeBData(buffer, false);
            continue;
        }

        std::vector<std::uint8_t> final_block(
            buffer.begin(), buffer.begin() + static_cast<std::size_t>(count));
        sendModeBData(final_block, true);
        break;
    }
    data_socket.reset();
}
