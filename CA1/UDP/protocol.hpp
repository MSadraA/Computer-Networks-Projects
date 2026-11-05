#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>
#include <cstdint>

namespace udp_pinger {
    constexpr size_t MAX_PAYLOAD_SIZE = 512;
    
    const std::string ERR_BAD_REQUEST = "ERR BAD_REQUEST";

    enum class MessageType {
        PING,
        PONG,
        ERR,
        UNKNOWN
    };

    struct Message {
        MessageType type;
        uint64_t seq;
        std::string token;
    };

}
#endif