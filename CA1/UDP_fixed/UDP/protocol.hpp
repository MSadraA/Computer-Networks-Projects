#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace udp_pinger {

constexpr std::size_t MAX_PAYLOAD_SIZE = 512;
const std::string ERR_BAD_REQUEST = "ERR BAD_REQUEST";

enum class MessageType {
    PING,
    PONG,
    UNKNOWN
};

struct Message {
    MessageType type;
    std::uint64_t seq;
    std::string token;

    Message() : type(MessageType::UNKNOWN), seq(0) {}
};

}

#endif
