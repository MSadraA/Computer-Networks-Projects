#include "MessageParser.hpp"

#include <cstdlib>

namespace {

bool isValidToken(const std::string& token) {
    if (token.empty() || token.size() > 64) {
        return false;
    }

    for (unsigned char ch : token) {
        if (ch < 33 || ch > 126) {
            return false;
        }
    }

    return true;
}

bool parseSequence(const std::string& text, std::uint64_t& value) {
    if (text.empty() || text.size() > 10) {
        return false;
    }

    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }

    value = 0;
    for (char ch : text) {
        value = value * 10 + static_cast<unsigned int>(ch - '0');
    }
    return true;
}

}

udp_pinger::Message MessageParser::deserialize(const std::string& raw_data) {
    udp_pinger::Message msg;

    if (raw_data.empty() || raw_data.size() > udp_pinger::MAX_PAYLOAD_SIZE) {
        return msg;
    }

    std::string data = raw_data;
    if (!data.empty() && data[data.size() - 1] == '\n') {
        data.erase(data.size() - 1);
    }

    // The protocol is ASCII. A newline is allowed only at the end.
    for (unsigned char ch : data) {
        if (ch < 32 || ch > 126) {
            return msg;
        }
    }

    const std::string::size_type first_space = data.find(' ');
    if (first_space == std::string::npos || first_space == 0) {
        return msg;
    }

    const std::string::size_type second_space = data.find(' ', first_space + 1);
    if (second_space == std::string::npos || second_space == first_space + 1 ||
        second_space == data.size() - 1 || data.find(' ', second_space + 1) != std::string::npos) {
        return msg;
    }

    const std::string command = data.substr(0, first_space);
    const std::string sequence = data.substr(first_space + 1, second_space - first_space - 1);
    const std::string token = data.substr(second_space + 1);

    if (command == "PING") {
        msg.type = udp_pinger::MessageType::PING;
    } else if (command == "PONG") {
        msg.type = udp_pinger::MessageType::PONG;
    } else {
        return udp_pinger::Message();
    }

    if (!parseSequence(sequence, msg.seq) || !isValidToken(token)) {
        return udp_pinger::Message();
    }

    msg.token = token;
    return msg;
}

std::string MessageParser::serializePing(std::uint64_t seq, const std::string& token) {
    return "PING " + std::to_string(seq) + " " + token;
}

std::string MessageParser::serializePong(std::uint64_t seq, const std::string& token) {
    return "PONG " + std::to_string(seq) + " " + token;
}
