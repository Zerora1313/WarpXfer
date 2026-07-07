#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include <string>
#include <cstdint>

namespace cli {

enum class Mode {
    NONE,
    SEND,
    RECEIVE,
    HELP
};

struct Config {
    Mode mode = Mode::NONE;
    std::string path;
    uint16_t port = 9001;
    uint16_t discovery_port = 9000;
    std::string manual_ip;
    bool manual = false;
};

Config ParseArgs(int argc, char* argv[]);
void PrintHelp();

} // namespace cli

#endif // CLI_PARSER_H
