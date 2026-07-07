#include "../include/discovery_service.h"
#include "../include/network_compat.h"
#include <cstring>
#include <iostream>
#include <chrono>

namespace network {

DiscoveryService::DiscoveryService() : m_running(false), m_socket(INVALID_SOCKET) {}

DiscoveryService::~DiscoveryService() {
    StopReceiver();
}

bool DiscoveryService::StartReceiver(uint16_t listen_port, const std::string& device_name, uint16_t tcp_port) {
    StopReceiver();

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Failed to create UDP socket: " << GetSocketErrorString() << std::endl;
        return false;
    }

    int optval = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&optval), sizeof(optval));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listen_port);

    if (bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind UDP socket: " << GetSocketErrorString() << std::endl;
        CloseSocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    m_running = true;
    try {
        m_thread = std::thread(&DiscoveryService::ReceiverLoop, this, device_name, tcp_port);
    } catch (const std::exception& e) {
        std::cerr << "Failed to spawn discovery thread: " << e.what() << std::endl;
        CloseSocket(m_socket);
        m_socket = INVALID_SOCKET;
        m_running = false;
        return false;
    }
    return true;
}

void DiscoveryService::StopReceiver() {
    m_running = false;
    if (m_socket != INVALID_SOCKET) {
        CloseSocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void DiscoveryService::ReceiverLoop(const std::string& device_name, uint16_t tcp_port) {
    uint8_t buffer[2048];
    sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    while (m_running) {
        int bytes_recvd = recvfrom(m_socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                   reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
        if (bytes_recvd <= 0) {
            if (m_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }

        if (bytes_recvd < 9) continue;
        if (std::memcmp(buffer, "WARP", 4) != 0) continue;

        uint8_t type = buffer[4];
        uint32_t len = ReadUint32(&buffer[5]);
        if (bytes_recvd < static_cast<int>(9 + len)) continue;

        if (type == static_cast<uint8_t>(PacketType::DISCOVER_REQ)) {
            // Reply: DISCOVER_RESP
            std::vector<uint8_t> payload(3 + device_name.size());
            payload[0] = (tcp_port >> 8) & 0xFF;
            payload[1] = tcp_port & 0xFF;
            payload[2] = static_cast<uint8_t>(device_name.size());
            std::memcpy(payload.data() + 3, device_name.data(), device_name.size());

            std::vector<uint8_t> packet(9 + payload.size());
            std::memcpy(&packet[0], "WARP", 4);
            packet[4] = static_cast<uint8_t>(PacketType::DISCOVER_RESP);
            WriteUint32(&packet[5], static_cast<uint32_t>(payload.size()));
            std::memcpy(packet.data() + 9, payload.data(), payload.size());

            sendto(m_socket, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
                   reinterpret_cast<sockaddr*>(&client_addr), client_addr_len);
        }
    }
}

std::vector<DiscoveredDevice> DiscoveryService::ScanNetwork(uint16_t target_port, int timeout_ms) {
    std::vector<DiscoveredDevice> devices;

    SocketType s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        std::cerr << "ScanNetwork: failed to create socket." << std::endl;
        return devices;
    }

    int broadcast = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast)) == SOCKET_ERROR) {
        std::cerr << "ScanNetwork: failed to enable SO_BROADCAST." << std::endl;
        CloseSocket(s);
        return devices;
    }

    sockaddr_in broadcast_addr;
    std::memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcast_addr.sin_port = htons(target_port);

    std::vector<uint8_t> packet(9);
    std::memcpy(&packet[0], "WARP", 4);
    packet[4] = static_cast<uint8_t>(PacketType::DISCOVER_REQ);
    WriteUint32(&packet[5], 0);

    if (sendto(s, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
               reinterpret_cast<sockaddr*>(&broadcast_addr), sizeof(broadcast_addr)) == SOCKET_ERROR) {
        std::cerr << "ScanNetwork: sendto failed: " << GetSocketErrorString() << std::endl;
        CloseSocket(s);
        return devices;
    }

    fd_set read_fds;
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    uint8_t buffer[2048];
    sockaddr_in reply_addr;
    int reply_addr_len = sizeof(reply_addr);

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(s, &read_fds);

        int activity = select(static_cast<int>(s + 1), &read_fds, NULL, NULL, &tv);
        if (activity <= 0) {
            break; // Timeout or error
        }

        if (FD_ISSET(s, &read_fds)) {
            int bytes_recvd = recvfrom(s, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                       reinterpret_cast<sockaddr*>(&reply_addr), &reply_addr_len);
            if (bytes_recvd < 9) continue;

            if (std::memcmp(buffer, "WARP", 4) != 0) continue;
            uint8_t type = buffer[4];
            uint32_t len = ReadUint32(&buffer[5]);

            if (type == static_cast<uint8_t>(PacketType::DISCOVER_RESP)) {
                if (len >= 3) {
                    uint16_t tcp_port = (buffer[9] << 8) | buffer[10];
                    uint8_t name_len = buffer[11];
                    if (len >= (uint32_t)(3 + name_len)) {
                        std::string name(reinterpret_cast<char*>(&buffer[12]), name_len);
                        
                        char* ip_str = inet_ntoa(reply_addr.sin_addr);
                        if (ip_str) {
                            bool exists = false;
                            for (const auto& dev : devices) {
                                if (dev.ip == ip_str && dev.port == tcp_port) {
                                    exists = true;
                                    break;
                                }
                            }

                            if (!exists) {
                                DiscoveredDevice dev;
                                dev.ip = ip_str;
                                dev.port = tcp_port;
                                dev.name = name;
                                devices.push_back(dev);
                            }
                        }
                    }
                }
            }
        }
    }

    CloseSocket(s);
    return devices;
}

} // namespace network
