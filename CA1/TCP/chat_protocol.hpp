#ifndef CHAT_PROTOCOL_HPP
#define CHAT_PROTOCOL_HPP

#include <string>
#include <set>
#include <cstdint>

namespace utchat {

constexpr size_t MAX_LINE_LENGTH = 1024;
constexpr const char* WELCOME_BANNER = "HELLO UT-CHAT/1.0";

inline bool is_valid_ident_char(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '_' || c == '-';
}

inline bool is_valid_nick(const std::string& nick) {
    if (nick.empty() || nick.size() > 16) {
        return false;
    }
    for (char c : nick) {
        if (!is_valid_ident_char(c)) {
            return false;
        }
    }
    return true;
}

inline bool is_valid_room(const std::string& room) {
    if (room.size() < 2 || room.size() > 32) {
        return false;
    }
    if (room[0] != '#') {
        return false;
    }
    for (size_t i = 1; i < room.size(); ++i) {
        if (!is_valid_ident_char(room[i])) {
            return false;
        }
    }
    return true;
}

inline bool is_known_command(const std::string& cmd) {
    static const std::set<std::string> valid_commands = {
        "NICK", "JOIN", "PART", "MSG", "PRIVMSG",
        "LIST", "WHO", "PING", "QUIT"
    };
    return valid_commands.find(cmd) != valid_commands.end();
}

} 

#endif