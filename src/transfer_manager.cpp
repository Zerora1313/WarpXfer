#include "../include/transfer_manager.h"
#include "../include/network_compat.h"
#include "../include/discovery_service.h"
#include "../include/receiver_engine.h"
#include "../include/sender_engine.h"
#include "../include/folder_scanner.h"
#include "../include/ui_utils.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>

namespace core {

static uint64_t GetCurrentTimeSeconds() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

static std::string FormatSize(uint64_t bytes) {
    double db = static_cast<double>(bytes);
    std::stringstream ss;
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        ss << std::fixed << std::setprecision(1) << (db / (1024.0 * 1024.0 * 1024.0)) << " GB";
    } else {
        ss << std::fixed << std::setprecision(0) << (db / (1024.0 * 1024.0)) << " MB";
    }
    return ss.str();
}

bool TransferManager::Run(const cli::Config& config) {
    ui::SetupConsoleColors();

    if (!network::InitSockets()) {
        std::cerr << "Error: Failed to initialize network subsystem." << std::endl;
        return false;
    }

    bool success = true;

    if (config.mode == cli::Mode::RECEIVE) {
        std::string my_name = ui::GenerateRandomName();
        uint16_t tcp_port = config.port;

        std::cout << ui::Magenta() << "┌──────────────────────────────────────────────────┐" << ui::Reset() << std::endl;
        std::cout << ui::Magenta() << "│         📡 WARPXFER v1.0 — RECEIVER MODE         │" << ui::Reset() << std::endl;
        std::cout << ui::Magenta() << "└──────────────────────────────────────────────────┘" << ui::Reset() << std::endl;
        std::cout << "\nYour ID: " << ui::Bold() << ui::Cyan() << my_name << ui::Reset() << std::endl;
        std::cout << "Port:    " << tcp_port << std::endl;

        network::DiscoveryService discovery;
        if (!discovery.StartReceiver(config.discovery_port, my_name, tcp_port)) {
            std::cerr << ui::Red() << "Warning: Failed to start UDP auto-discovery responder." << ui::Reset() << std::endl;
        }

        ReceiverEngine receiver;
        std::string save_dir = config.path.empty() ? "." : config.path;

        if (receiver.StartServer(tcp_port, save_dir, my_name)) {
            std::cout << "📡 Listening... Waiting for sender (Auto-exit on 2.5 min inactivity) [Press Ctrl+C to exit]\n" << std::endl;
            const uint64_t TIMEOUT_SECONDS = 150;
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (receiver.GetActiveTransfersCount() == 0) {
                    uint64_t elapsed = GetCurrentTimeSeconds() - receiver.GetLastActivityTime();
                    if (elapsed >= TIMEOUT_SECONDS) {
                        std::cout << "\n⏰ Receiver timed out after 2.5 minutes of inactivity. Exiting..." << std::endl;
                        break;
                    }
                }
            }
            receiver.StopServer();
        } else {
            std::cerr << ui::Red() << "Error: Failed to start TCP data listener." << ui::Reset() << std::endl;
            success = false;
        }

        discovery.StopReceiver();

    } else if (config.mode == cli::Mode::SEND) {
        std::cout << ui::Yellow() << "┌──────────────────────────────────────────────────┐" << ui::Reset() << std::endl;
        std::cout << ui::Yellow() << "│          🚀 WARPXFER v1.0 — SENDER MODE          │" << ui::Reset() << std::endl;
        std::cout << ui::Yellow() << "└──────────────────────────────────────────────────┘" << ui::Reset() << std::endl << std::endl;

        fileio::ScanResult scan = fileio::ScanFolderOrFile(config.path);
        if (scan.total_files == 0) {
            std::cout << ui::Red() << "❌ File or folder not found: " << config.path << ui::Reset() << std::endl;
            network::CleanupSockets();
            return false;
        }

        std::cout << ui::Bold() << "📁 " << fileio::NormalizePath(config.path) << ui::Reset() << std::endl;
        if (scan.is_single_file) {
            std::cout << "   1 file | " << FormatSize(scan.total_size) << std::endl;
        } else {
            std::cout << "   " << scan.total_files << " files | " << scan.total_folders << " folders | " << FormatSize(scan.total_size) << std::endl;
        }

        std::cout << "   Types: ";
        int ext_index = 0;
        for (const auto& pair : scan.type_counts) {
            if (ext_index > 0) std::cout << "  ";
            std::cout << pair.first << " (" << pair.second << ")";
            ext_index++;
        }
        std::cout << "\n\n";

        std::string target_ip = config.manual_ip;
        uint16_t target_port = config.port;

        if (!config.manual) {
            std::cout << ui::Yellow() << "🔍 Scanning local network for active receivers (5s)..." << ui::Reset() << std::endl;
            std::vector<network::DiscoveredDevice> devices = network::DiscoveryService::ScanNetwork(config.discovery_port, 5000);

            int choice = ui::SelectDevice(devices);
            if (choice < 0) {
                network::CleanupSockets();
                return false;
            }

            target_ip = devices[choice].ip;
            target_port = devices[choice].port;
        }

        std::string sender_name = ui::GenerateRandomName();

        SenderEngine sender;
        success = sender.ConnectAndTransfer(target_ip, target_port, config.path, sender_name);
    } else {
        cli::PrintHelp();
    }

    network::CleanupSockets();
    return success;
}

} // namespace core
