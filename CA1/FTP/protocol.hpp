#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace ftp_protocol {

constexpr std::size_t MAX_COMMAND_LENGTH = 1024;
constexpr std::size_t MAX_BLOCK_DATA = 1024;

constexpr int RESP_READY = 220;
constexpr int RESP_PASS_REQ = 331;
constexpr int RESP_LOGGED_IN = 230;
constexpr int RESP_OK = 200;
constexpr int RESP_PASV = 227;
constexpr int RESP_DATA_OPEN = 150;
constexpr int RESP_DATA_DONE = 226;
constexpr int RESP_GOODBYE = 221;

constexpr int ERR_UNKNOWN = 500;
constexpr int ERR_SYNTAX = 501;
constexpr int ERR_SEQUENCE = 503;
constexpr int ERR_NO_IMPL = 504;
constexpr int ERR_NOT_LOGGED_IN = 530;
constexpr int ERR_BAD_NAME = 553;
constexpr int ERR_NO_DATA_CONN = 425;
constexpr int ERR_NOT_FOUND = 550;
constexpr int ERR_CONN_CLOSED = 426;

constexpr std::uint8_t DESC_DATA = 0x00;
constexpr std::uint8_t DESC_EOF = 0x40;

enum class CommandType {
    USER, PASS, MODE, PASV, LIST, RETR, STOR, QUIT, UNKNOWN
};

struct ControlMessage {
    CommandType type;
    std::string argument;

    ControlMessage() : type(CommandType::UNKNOWN) {}
};

}

#endif
