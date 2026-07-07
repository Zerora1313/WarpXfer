#ifndef DISCOVERY_SERVICE_H
#define DISCOVERY_SERVICE_H

#include "network_compat.h"
#include <string>
#include <vector>
#include <atomic>
#include <thread>

namespace network {

struct DiscoveredDevice {
    std::string ip;
    uint16_t port;
    std::string name;
};

class DiscoveryService {
public:
    DiscoveryService();
    ~DiscoveryService();

    // Starts background UDP server listening for discovery requests
    bool StartReceiver(uint16_t listen_port, const std::string& device_name, uint16_t tcp_port);
    void StopReceiver();

    // Sends a broadcast and returns list of discovered devices
    static std::vector<DiscoveredDevice> ScanNetwork(uint16_t target_port, int timeout_ms);
    void ReceiverLoop(const std::string& device_name, uint16_t tcp_port);

private:
    std::atomic<bool> m_running;
    std::thread m_thread;
    SocketType m_socket;
};

} // namespace network

#endif // DISCOVERY_SERVICE_H
