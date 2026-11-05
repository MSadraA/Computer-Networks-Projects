#include "MessageParser.hpp"
#include <sstream>

udp_pinger::Message MessageParser::deserialize(const std::string& raw_data) {
    udp_pinger::Message msg;
    msg.type = udp_pinger::MessageType::UNKNOWN;

    if (raw_data.length() > udp_pinger::MAX_PAYLOAD_SIZE) {
        return msg;
    }

    std::stringstream ss(raw_data);
    std::string command;
    ss >> command;

    if (command == "PING") {
        msg.type = udp_pinger::MessageType::PING;
    } else if (command == "PONG") {
        msg.type = udp_pinger::MessageType::PONG;
    } else {
        return msg;
    }

    if (!(ss >> msg.seq)) {
        msg.type = udp_pinger::MessageType::UNKNOWN;
        return msg;
    }

    if (!(ss >> msg.token)) {
        msg.type = udp_pinger::MessageType::UNKNOWN;
        return msg;
    }

    return msg;
}

std::string MessageParser::serializePing(uint64_t seq, const std::string& token) {
    return "PING " + std::to_string(seq) + " " + token;
}

std::string MessageParser::serializePong(uint64_t seq, const std::string& token) {
    return "PONG " + std::to_string(seq) + " " + token;
}