#ifndef MESSAGEPARSER_HPP
#define MESSAGEPARSER_HPP

#include "protocol.hpp"
#include <string>

class MessageParser {
public:
    static udp_pinger::Message deserialize(const std::string& raw_data);

    static std::string serializePing(std::uint64_t seq, const std::string& token);
    static std::string serializePong(std::uint64_t seq, const std::string& token);
};

#endif
