#ifndef FTPPARSER_HPP
#define FTPPARSER_HPP

#include "protocol.hpp"
#include <cstdint>
#include <string>
#include <vector>

class FTPParser {
public:
    static ftp_protocol::ControlMessage parseCommand(const std::string& raw_line);

    static bool isValidFileName(const std::string& name);

    static std::string formatReply(int code, const std::string& message);
    
    static std::vector<std::uint8_t> createModeBHeader(std::uint8_t descriptor, std::uint16_t length);
};

#endif