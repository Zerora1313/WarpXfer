#ifndef SENDER_ENGINE_H
#define SENDER_ENGINE_H

#include "network_compat.h"
#include "folder_scanner.h"
#include <string>
#include <vector>

namespace core {

class SenderEngine {
public:
    SenderEngine();
    ~SenderEngine();

    bool ConnectAndTransfer(const std::string& ip, uint16_t port, const std::string& source_path, const std::string& sender_name);

private:
    SocketType m_socket;

    bool Handshake(const fileio::ScanResult& scan, const std::string& sender_name, std::vector<uint64_t>& resume_offsets);
    bool SendFilesMetadata(const fileio::ScanResult& scan, std::vector<uint64_t>& resume_offsets);
    bool TransferData(const fileio::ScanResult& scan, const std::vector<uint64_t>& resume_offsets);
};

} // namespace core

#endif // SENDER_ENGINE_H
