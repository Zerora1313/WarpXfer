#include "../include/sender_engine.h"
#include "../include/network_compat.h"
#include "../include/sha256_hasher.h"
#include "../include/ui_utils.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <memory>

namespace core {

SenderEngine::SenderEngine() : m_socket(INVALID_SOCKET) {}

SenderEngine::~SenderEngine() {
    if (m_socket != INVALID_SOCKET) {
        CloseSocket(m_socket);
    }
}

bool SenderEngine::ConnectAndTransfer(const std::string& ip, uint16_t port, const std::string& source_path, const std::string& sender_name) {
    std::cout << "\n⚡ Connecting to " << ip << ":" << port << "..." << ui::Reset() << std::endl;
    
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        std::cout << ui::Red() << "❌ Failed to create TCP socket: " << network::GetSocketErrorString() << ui::Reset() << std::endl;
        return false;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);

    if (connect(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        bool refused = (err == 10061);
#else
        int err = errno;
        bool refused = (err == ECONNREFUSED);
#endif
        if (refused) {
            std::cout << ui::Red() << "❌ Connect to " << ip << ":" << port << " failed: Connection refused.\n"
                      << "   👉 Make sure the receiver is open, listening on port " << port << ",\n"
                      << "      and your firewall is not blocking the connection." << ui::Reset() << std::endl;
        } else {
            std::cout << ui::Red() << "❌ Connect to " << ip << ":" << port << " failed: " << network::GetSocketErrorString() << ui::Reset() << std::endl;
        }
        CloseSocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    std::cout << ui::Green() << "⚡ Connected successfully!\n" << ui::Reset() << std::endl;

    fileio::ScanResult scan = fileio::ScanFolderOrFile(source_path);
    if (scan.total_files == 0) {
        std::cout << ui::Red() << "❌ No files found to transfer." << ui::Reset() << std::endl;
        return false;
    }

    std::vector<uint64_t> resume_offsets(scan.files.size(), 0);
    if (!Handshake(scan, sender_name, resume_offsets)) {
        return false;
    }

    if (!SendFilesMetadata(scan, resume_offsets)) {
        std::cout << ui::Red() << "❌ Failed to send file metadata." << ui::Reset() << std::endl;
        return false;
    }

    std::cout << "──────────────────────────────────────────────────" << std::endl;
    std::cout << ui::Bold() << "📦 Transferring: " << scan.root_name << " (Press Ctrl+C to cancel)\n" << ui::Reset() << std::endl;
    if (!TransferData(scan, resume_offsets)) {
        std::cout << ui::Red() << "❌ Transfer failed: Connection with receiver lost or aborted." << ui::Reset() << std::endl;
        std::cout << "──────────────────────────────────────────────────\n" << std::endl;
        return false;
    }

    return true;
}

bool SenderEngine::Handshake(const fileio::ScanResult& scan, const std::string& sender_name, std::vector<uint64_t>& resume_offsets) {
    std::vector<uint8_t> payload;
    payload.resize(4 + 8 + 2 + scan.root_name.size() + 2 + sender_name.size());

    uint8_t* ptr = payload.data();
    network::WriteUint32(ptr, scan.total_files); ptr += 4;
    network::WriteUint64(ptr, scan.total_size); ptr += 8;

    uint16_t root_len = static_cast<uint16_t>(scan.root_name.size());
    ptr[0] = (root_len >> 8) & 0xFF;
    ptr[1] = root_len & 0xFF;
    ptr += 2;
    std::memcpy(ptr, scan.root_name.data(), scan.root_name.size());
    ptr += scan.root_name.size();

    uint16_t sender_len = static_cast<uint16_t>(sender_name.size());
    ptr[0] = (sender_len >> 8) & 0xFF;
    ptr[1] = sender_len & 0xFF;
    ptr += 2;
    std::memcpy(ptr, sender_name.data(), sender_name.size());

    if (!network::SendPacket(m_socket, network::PacketType::SESSION_START_REQ, payload)) {
        return false;
    }

    std::cout << ui::Yellow() << "⏳ Waiting for receiver to accept the transfer..." << ui::Reset() << std::endl;

    network::PacketType resp_type;
    std::vector<uint8_t> resp_payload;
    if (!network::ReceivePacket(m_socket, resp_type, resp_payload)) {
        std::cout << ui::Red() << "❌ Handshake failed: Connection closed by receiver." << ui::Reset() << std::endl;
        return false;
    }

    if (resp_type != network::PacketType::SESSION_START_RESP || resp_payload.empty()) {
        std::cout << ui::Red() << "❌ Handshake failed: Invalid response from receiver." << ui::Reset() << std::endl;
        return false;
    }

    uint8_t accepted = resp_payload[0];

    // Parse receiver name
    std::string receiver_name = "Unknown Receiver";
    size_t name_offset = 1;
    if (resp_payload.size() >= 3) {
        uint16_t name_len = (resp_payload[1] << 8) | resp_payload[2];
        if (resp_payload.size() >= static_cast<size_t>(3 + name_len)) {
            receiver_name = std::string(reinterpret_cast<const char*>(resp_payload.data() + 3), name_len);
            name_offset = 3 + name_len;
        }
    }

    if (accepted == 0) {
        std::cout << ui::Red() << "❌ Rejection: The receiver (" << receiver_name << ") declined the transfer request." << ui::Reset() << std::endl;
        return false;
    }

    // Now parse resume count if present
    if (resp_payload.size() >= name_offset + 4) {
        uint32_t resume_count = network::ReadUint32(resp_payload.data() + name_offset);
        size_t expected_size = name_offset + 4 + resume_count * 12;
        if (resp_payload.size() >= expected_size) {
            const uint8_t* offset_ptr = resp_payload.data() + name_offset + 4;
            for (uint32_t i = 0; i < resume_count; ++i) {
                uint32_t file_id = network::ReadUint32(offset_ptr); offset_ptr += 4;
                uint64_t offset = network::ReadUint64(offset_ptr); offset_ptr += 8;
                if (file_id < resume_offsets.size()) {
                    resume_offsets[file_id] = offset;
                }
            }
        }
    }

    return true;
}

bool SenderEngine::SendFilesMetadata(const fileio::ScanResult& scan, std::vector<uint64_t>& resume_offsets) {
    for (uint32_t i = 0; i < scan.files.size(); ++i) {
        const auto& file = scan.files[i];
        
        std::vector<uint8_t> payload(4 + 8 + 2 + file.relative_path.size());
        uint8_t* ptr = payload.data();
        network::WriteUint32(ptr, i); ptr += 4;
        network::WriteUint64(ptr, file.size); ptr += 8;

        uint16_t path_len = static_cast<uint16_t>(file.relative_path.size());
        ptr[0] = (path_len >> 8) & 0xFF;
        ptr[1] = path_len & 0xFF;
        ptr += 2;
        std::memcpy(ptr, file.relative_path.data(), file.relative_path.size());

        if (!network::SendPacket(m_socket, network::PacketType::FILE_METADATA, payload)) {
            return false;
        }

        network::PacketType ack_type;
        std::vector<uint8_t> ack_payload;
        if (!network::ReceivePacket(m_socket, ack_type, ack_payload)) {
            return false;
        }

        if (ack_type != network::PacketType::CHUNK_ACK || ack_payload.size() < 13) {
            return false;
        }

        uint32_t ack_file_id = network::ReadUint32(ack_payload.data());
        uint64_t resume_offset = network::ReadUint64(ack_payload.data() + 4);
        uint8_t status = ack_payload[12];

        if (ack_file_id == i && status == 1) {
            resume_offsets[i] = resume_offset;
        }
    }
    return true;
}

bool SenderEngine::TransferData(const fileio::ScanResult& scan, const std::vector<uint64_t>& resume_offsets) {
    ui::ProgressBar progress;
    
    uint64_t total_transferred = 0;
    uint32_t completed_files = 0;
    for (uint32_t i = 0; i < scan.files.size(); ++i) {
        total_transferred += resume_offsets[i];
        if (resume_offsets[i] >= scan.files[i].size) {
            completed_files++;
        }
    }

    progress.Start(scan.total_size, scan.total_files);
    progress.Update(total_transferred, completed_files);

    const int CHUNK_SIZE = 8192;
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[CHUNK_SIZE]);

    for (uint32_t i = 0; i < scan.files.size(); ++i) {
        const auto& file_info = scan.files[i];
        uint64_t offset = resume_offsets[i];

        if (offset >= file_info.size) {
            continue;
        }

        // Inline File IO read stream
        std::ifstream file(file_info.absolute_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "\nFailed to open source file: " << file_info.absolute_path << std::endl;
            return false;
        }

        file.seekg(offset, std::ios::beg);

        while (offset < file_info.size) {
            uint64_t remaining = file_info.size - offset;
            int to_read = (remaining < (uint64_t)CHUNK_SIZE) ? (int)remaining : CHUNK_SIZE;

            file.read(reinterpret_cast<char*>(buffer.get()), to_read);
            int read_bytes = static_cast<int>(file.gcount());
            if (read_bytes <= 0) break;

            std::string hash = crypto::CalculateSHA256(buffer.get(), read_bytes);

            std::vector<uint8_t> payload(4 + 8 + 4 + 64 + read_bytes);
            uint8_t* ptr = payload.data();
            network::WriteUint32(ptr, i); ptr += 4;
            network::WriteUint64(ptr, offset); ptr += 8;
            network::WriteUint32(ptr, static_cast<uint32_t>(read_bytes)); ptr += 4;
            std::memcpy(ptr, hash.c_str(), 64); ptr += 64;
            std::memcpy(ptr, buffer.get(), read_bytes);

            if (!network::SendPacket(m_socket, network::PacketType::FILE_CHUNK, payload)) {
                return false;
            }

            network::PacketType ack_type;
            std::vector<uint8_t> ack_payload;
            if (!network::ReceivePacket(m_socket, ack_type, ack_payload)) {
                return false;
            }

            if (ack_type != network::PacketType::CHUNK_ACK || ack_payload.size() < 13) {
                return false;
            }

            uint32_t ack_file_id = network::ReadUint32(ack_payload.data());
            uint64_t ack_offset = network::ReadUint64(ack_payload.data() + 4);
            uint8_t ack_status = ack_payload[12];

            if (ack_file_id != i || ack_offset != offset || ack_status == 0) {
                std::cerr << "\nChunk ACK verification failed for file " << i << " offset " << offset << std::endl;
                return false;
            }

            offset += read_bytes;
            total_transferred += read_bytes;
            progress.Update(total_transferred, completed_files);
        }

        file.close();
        completed_files++;
        progress.Update(total_transferred, completed_files);
    }

    std::vector<uint8_t> end_payload;
    network::SendPacket(m_socket, network::PacketType::SESSION_END, end_payload);

    progress.Done();
    std::cout << "\n" << ui::Green() << "✅ Success! Transferred " << progress.FormatBytes(scan.total_size) << " successfully." << ui::Reset() << std::endl;
    std::cout << "──────────────────────────────────────────────────\n" << std::endl;
    return true;
}

} // namespace core
