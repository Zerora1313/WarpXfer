#include "../include/cli_parser.h"
#include <iostream>
#include <cstdlib>

namespace cli {

void PrintHelp() {
    std::cout << "🚀 WarpXfer v1.0 — Terminal-based P2P file transfer tool\n\n";
    std::cout << "Usage:\n";
    std::cout << "  warp send <file_or_folder> [options]\n";
    std::cout << "  warp receive [options]\n";
    std::cout << "  warp --help | -h\n\n";
    std::cout << "Options:\n";
    std::cout << "  --port <port>       Port to use for TCP data transfer (default: 9001)\n";
    std::cout << "  --manual <ip>       Directly connect to receiver IP (skips UDP discovery)\n";
    std::cout << "  --dir <path>        Directory to save files (for receiver, default: current directory)\n";
    std::cout << "  --udpport <port>    UDP port used for auto-discovery (default: 9000)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  warp send D:\\schoolProject\\\n";
    std::cout << "  warp send D:\\movie.mp4 --manual 192.168.1.10\n";
    std::cout << "  warp receive --dir C:\\Downloads\\warp-received\\\n";
}

Config ParseArgs(int argc, char* argv[]) {
    Config config;
    if (argc < 2) {
        config.mode = Mode::HELP;
        return config;
    }

    std::string first_arg = argv[1];
    if (first_arg == "send") {
        config.mode = Mode::SEND;
        if (argc < 3) {
            std::cerr << "Error: 'send' mode requires a file or folder path.\n";
            config.mode = Mode::HELP;
            return config;
        }
        config.path = argv[2];
    } else if (first_arg == "receive") {
        config.mode = Mode::RECEIVE;
    } else if (first_arg == "--help" || first_arg == "-h" || first_arg == "help") {
        config.mode = Mode::HELP;
        return config;
    } else {
        std::cerr << "Error: Unknown command '" << first_arg << "'\n";
        config.mode = Mode::HELP;
        return config;
    }

    int start_idx = (config.mode == Mode::SEND) ? 3 : 2;
    for (int i = start_idx; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port") {
            if (i + 1 < argc) {
                config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
            } else {
                std::cerr << "Error: --port requires an argument\n";
                config.mode = Mode::HELP;
            }
        } else if (arg == "--manual") {
            if (i + 1 < argc) {
                config.manual_ip = argv[++i];
                config.manual = true;
            } else {
                std::cerr << "Error: --manual requires an IP address\n";
                config.mode = Mode::HELP;
            }
        } else if (arg == "--dir") {
            if (config.mode == Mode::RECEIVE) {
                if (i + 1 < argc) {
                    config.path = argv[++i];
                } else {
                    std::cerr << "Error: --dir requires a path\n";
                    config.mode = Mode::HELP;
                }
            } else {
                std::cerr << "Warning: --dir flag ignored in send mode\n";
                i++;
            }
        } else if (arg == "--udpport") {
            if (i + 1 < argc) {
                config.discovery_port = static_cast<uint16_t>(std::atoi(argv[++i]));
            } else {
                std::cerr << "Error: --udpport requires an argument\n";
                config.mode = Mode::HELP;
            }
        } else {
            std::cerr << "Warning: Unknown option '" << arg << "' ignored\n";
        }
    }

    return config;
}

} // namespace cli
