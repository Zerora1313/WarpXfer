#ifndef RECEIVER_ENGINE_H
#define RECEIVER_ENGINE_H

#include "network_compat.h"
#include <string>
#include <atomic>
#include <thread>
#include <cstdint>

namespace core {

struct ReceivedFileMeta {
    uint32_t file_id;
    uint64_t total_size;
    std::string relative_path;
};

class ReceiverEngine {
public:
    ReceiverEngine();
    ~ReceiverEngine();

    bool StartServer(uint16_t port, const std::string& save_dir, const std::string& device_name);
    void StopServer();
    bool IsRunning() const { return m_running; }

    uint64_t GetLastActivityTime() const { return m_last_activity_seconds.load(); }
    int GetActiveTransfersCount() const { return m_active_transfers.load(); }

    void ListenLoop(const std::string& save_dir, const std::string& device_name);
    void HandleClient(SocketType s, const std::string& client_ip, const std::string& save_dir, const std::string& device_name);

private:
    std::atomic<bool> m_running;
    SocketType m_listen_socket;
    std::thread m_thread;
    std::atomic<uint64_t> m_last_activity_seconds;
    std::atomic<int> m_active_transfers;
};

} // namespace core

#endif // RECEIVER_ENGINE_H
