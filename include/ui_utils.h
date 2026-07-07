#ifndef UI_UTILS_H
#define UI_UTILS_H

#include "discovery_service.h"
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace ui {

// Colors
void SetupConsoleColors();
std::string Reset();
std::string Green();
std::string Red();
std::string Yellow();
std::string Blue();
std::string Cyan();
std::string Magenta();
std::string Bold();

// Device selection and random names
std::string GenerateRandomName();
int SelectDevice(const std::vector<network::DiscoveredDevice>& devices);

// Progress Bar
class ProgressBar {
public:
    ProgressBar();
    void Start(uint64_t total_bytes, uint32_t total_files);
    void Update(uint64_t current_bytes, uint32_t current_files);
    void Done();

    std::string FormatBytes(uint64_t bytes) const;
private:
    uint64_t m_total_bytes;
    uint32_t m_total_files;
    uint64_t m_last_bytes;

    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_update_time;
    double m_current_speed; // in MB/s
};

} // namespace ui

#endif // UI_UTILS_H
