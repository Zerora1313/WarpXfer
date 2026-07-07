#include "../include/ui_utils.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <random>
#include <conio.h>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ui {

// Colors implementation
void SetupConsoleColors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    
    dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
    SetConsoleMode(hOut, dwMode);
    
    SetConsoleOutputCP(65001); // UTF-8 support
#endif
}

std::string Reset()   { return "\033[0m"; }
std::string Green()   { return "\033[32m"; }
std::string Red()     { return "\033[31m"; }
std::string Yellow()  { return "\033[33m"; }
std::string Blue()    { return "\033[34m"; }
std::string Cyan()    { return "\033[36m"; }
std::string Magenta() { return "\033[35m"; }
std::string Bold()    { return "\033[1m"; }

// Device List Selector & Random Name Implementation
std::string GenerateRandomName() {
    static const std::vector<std::string> colors = {
        "blue", "red", "cool", "swift", "neon", "brave", "happy", "mighty", "sunny", "wild",
        "frosty", "golden", "silent", "lucky", "dark", "cosmic", "crying", "speedy", "fancy", "crazy"
    };

    static const std::vector<std::string> animals = {
        "fox", "tiger", "panda", "eagle", "cat", "lion", "dog", "bull", "deer", "horse",
        "hawk", "wolf", "dragon", "bear", "owl", "seal", "lynx", "falcon", "panther", "raven"
    };

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> color_dist(0, static_cast<int>(colors.size() - 1));
    std::uniform_int_distribution<> animal_dist(0, static_cast<int>(animals.size() - 1));
    std::uniform_int_distribution<> num_dist(1, 9);

    int color_idx = color_dist(gen);
    int animal_idx = animal_dist(gen);
    int num = num_dist(gen);

    return colors[color_idx] + "-" + animals[animal_idx] + "-" + std::to_string(num);
}

int SelectDevice(const std::vector<network::DiscoveredDevice>& devices) {
    if (devices.empty()) {
        std::cout << "\n" << ui::Red() << "❌ No devices found" << ui::Reset() << std::endl;
        std::cout << "   👉 Please make sure the WarpXfer application is running in \"RECEIVER MODE\" on the target device." << std::endl;
        std::cout << "   👉 Ensure both devices are connected to the same Wi-Fi network or hotspot." << std::endl;
        std::cout << "   👉 To scan again, please restart the WarpXfer sender.\n" << std::endl;
        return -1;
    }

    std::cout << "\n──────────────────────────────────────────────────" << std::endl;
    std::cout << ui::Bold() << "🔍 Found active devices:" << ui::Reset() << std::endl;
    for (size_t i = 0; i < devices.size(); ++i) {
        std::cout << "   " << (i + 1) << ". " << ui::Green() << "🟢 " 
                  << std::left << std::setw(18) << devices[i].name 
                  << ui::Reset() << " (" << devices[i].ip << ":" << devices[i].port << ")" << std::endl;
    }
    std::cout << "──────────────────────────────────────────────────" << std::endl;

    std::cout << "\nSelect device (1-" << devices.size() << ") [Timeout in 60s]: " << std::flush;

    int timeout_seconds = 60;
    int elapsed_ms = 0;
    std::string response = "";

    while (elapsed_ms < timeout_seconds * 1000) {
        if (_kbhit()) {
            int ch = _getch();
            if (ch == '\r' || ch == '\n') {
                std::cout << std::endl;
                break;
            } else if (ch == '\b') { // Backspace
                if (!response.empty()) {
                    response.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            } else if (ch >= '0' && ch <= '9') {
                response += static_cast<char>(ch);
                std::cout << static_cast<char>(ch) << std::flush;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        elapsed_ms += 50;
    }

    if (elapsed_ms >= timeout_seconds * 1000) {
        std::cout << "\n" << ui::Yellow() << "⏰ Selection timed out. Exiting..." << ui::Reset() << std::endl;
        return -1;
    }

    if (response.empty()) {
        std::cout << ui::Red() << "No selection made. Exiting." << ui::Reset() << std::endl;
        return -1;
    }

    int choice = std::stoi(response);
    if (choice < 1 || choice > static_cast<int>(devices.size())) {
        std::cout << ui::Red() << "Invalid choice." << ui::Reset() << std::endl;
        return -1;
    }

    return choice - 1;
}

// ProgressBar Implementation
ProgressBar::ProgressBar() : m_total_bytes(0), m_total_files(0), m_last_bytes(0), m_current_speed(0.0) {}

void ProgressBar::Start(uint64_t total_bytes, uint32_t total_files) {
    m_total_bytes = total_bytes;
    m_total_files = total_files;
    m_last_bytes = 0;
    m_current_speed = 0.0;
    m_start_time = std::chrono::steady_clock::now();
    m_last_update_time = m_start_time;
}

std::string ProgressBar::FormatBytes(uint64_t bytes) const {
    double db = static_cast<double>(bytes);
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (db / (1024.0 * 1024.0 * 1024.0)) << " GB";
        return ss.str();
    } else if (bytes >= 1024ULL * 1024ULL) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(0) << (db / (1024.0 * 1024.0)) << " MB";
        return ss.str();
    } else if (bytes >= 1024ULL) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (db / 1024.0) << " KB";
        return ss.str();
    } else {
        return std::to_string(bytes) + " B";
    }
}

void ProgressBar::Update(uint64_t current_bytes, uint32_t current_files) {
    if (m_total_bytes == 0) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed_total_sec = std::chrono::duration_cast<std::chrono::duration<double>>(now - m_start_time).count();
    auto elapsed_delta_sec = std::chrono::duration_cast<std::chrono::duration<double>>(now - m_last_update_time).count();

    if (elapsed_delta_sec > 0.05) {
        uint64_t delta_bytes = current_bytes - m_last_bytes;
        double instant_speed = (static_cast<double>(delta_bytes) / (1024.0 * 1024.0)) / elapsed_delta_sec;
        
        if (m_current_speed == 0.0) {
            m_current_speed = instant_speed;
        } else {
            m_current_speed = 0.8 * m_current_speed + 0.2 * instant_speed;
        }

        m_last_bytes = current_bytes;
        m_last_update_time = now;
    } else if (m_current_speed == 0.0 && elapsed_total_sec > 0) {
        m_current_speed = (static_cast<double>(current_bytes) / (1024.0 * 1024.0)) / elapsed_total_sec;
    }

    double percent = (static_cast<double>(current_bytes) / m_total_bytes) * 100.0;
    if (percent > 100.0) percent = 100.0;

    double eta = 999.0;
    if (m_current_speed > 0.01) {
        uint64_t remaining_bytes = (m_total_bytes > current_bytes) ? (m_total_bytes - current_bytes) : 0;
        eta = (static_cast<double>(remaining_bytes) / (1024.0 * 1024.0)) / m_current_speed;
    }

    int bar_width = 20;
    int filled_width = static_cast<int>(std::round((percent / 100.0) * bar_width));
    if (filled_width > bar_width) filled_width = bar_width;

    std::string bar_str = "[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled_width) {
            bar_str += "█";
        } else {
            bar_str += "░";
        }
    }
    bar_str += "]";

    std::string eta_str = "ETA: --";
    if (eta < 999.0 && eta >= 0.0) {
        eta_str = "ETA: " + std::to_string(static_cast<int>(std::round(eta))) + "s";
    } else if (current_bytes >= m_total_bytes) {
        eta_str = "ETA: 0s";
    }

    std::stringstream speed_ss;
    speed_ss << std::fixed << std::setprecision(1) << m_current_speed << " MB/s";

    std::cout << "\r" << ui::Green() << bar_str << ui::Reset() 
              << " " << std::fixed << std::setprecision(0) << percent << "%"
              << "  " << speed_ss.str()
              << "  " << eta_str;

    std::cout << "\n\rFiles: " << current_files << "/" << m_total_files << " done  |  "
              << FormatBytes(current_bytes) << " / " << FormatBytes(m_total_bytes) << "          ";
              
    std::cout << "\033[F";
    std::cout.flush();
}

void ProgressBar::Done() {
    std::cout << "\n\n" << std::flush;
}

} // namespace ui
