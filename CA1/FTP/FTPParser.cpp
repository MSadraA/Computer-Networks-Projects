#include "FTPParser.hpp"
#include <algorithm>
#include <cctype>

ftp_protocol::ControlMessage FTPParser::parseCommand(const std::string& raw_line) {
    ftp_protocol::ControlMessage message;
    std::string line = raw_line;

    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    if (line.empty()) return message;

    const std::string::size_type separator = line.find(' ');
    std::string command = separator == std::string::npos
        ? line : line.substr(0, separator);
    if (separator != std::string::npos) {
        message.argument = line.substr(separator + 1);
    }

    std::transform(command.begin(), command.end(), command.begin(),
        [](unsigned char value) { return static_cast<char>(std::toupper(value)); });

    using ftp_protocol::CommandType;
    if (command == "USER") message.type = CommandType::USER;
    else if (command == "PASS") message.type = CommandType::PASS;
    else if (command == "MODE") message.type = CommandType::MODE;
    else if (command == "PASV") message.type = CommandType::PASV;
    else if (command == "LIST") message.type = CommandType::LIST;
    else if (command == "RETR") message.type = CommandType::RETR;
    else if (command == "STOR") message.type = CommandType::STOR;
    else if (command == "QUIT") message.type = CommandType::QUIT;

    return message;
}

bool FTPParser::isValidFileName(const std::string& name) {
    if (name.empty() || name.size() > 64) return false;
    for (unsigned char value : name) {
        if (!((value >= 'A' && value <= 'Z') ||
              (value >= 'a' && value <= 'z') ||
              (value >= '0' && value <= '9') ||
              value == '.' || value == '_' || value == '-')) {
            return false;
        }
    }
    return true;
}

std::string FTPParser::formatReply(int code, const std::string& message) {
    return std::to_string(code) + " " + message + "\r\n";
}

std::vector<std::uint8_t> FTPParser::createModeBHeader(
    std::uint8_t descriptor, std::uint16_t length) {
    std::vector<std::uint8_t> header(3);
    header[0] = descriptor;
    header[1] = static_cast<std::uint8_t>((length >> 8) & 0xFF);
    header[2] = static_cast<std::uint8_t>(length & 0xFF);
    return header;
}
