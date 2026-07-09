#include "../include/receiver_engine.h"
#include "../include/network_compat.h"
#include "../include/sha256_hasher.h"
#include "../include/folder_scanner.h"
#include "../include/ui_utils.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <chrono>
#include <conio.h>
#include <filesystem>
#include <vector>

namespace core {

static void InitializeHashForResume(crypto::SHA256& sha, const std::string& filepath, uint64_t resume_offset) {
    sha.Init();
    if (resume_offset == 0) return;
    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) return;
    const size_t buf_size = 64 * 1024;
    std::vector<uint8_t> buffer(buf_size);
    uint64_t read_total = 0;
    while (read_total < resume_offset) {
        uint64_t to_read = (resume_offset - read_total < buf_size) ? (resume_offset - read_total) : buf_size;
        f.read(reinterpret_cast<char*>(buffer.data()), to_read);
        std::streamsize bytes = f.gcount();
        if (bytes <= 0) break;
        sha.Update(buffer.data(), bytes);
        read_total += bytes;
    }
}

static uint64_t GetCurrentTimeSeconds() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

static uint64_t GetLocalFileSize(const std::string& filepath) {
    std::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return 0;
    return static_cast<uint64_t>(f.tellg());
}

ReceiverEngine::ReceiverEngine() 
    : m_running(false), m_listen_socket(INVALID_SOCKET), m_last_activity_seconds(0), m_active_transfers(0) {}

ReceiverEngine::~ReceiverEngine() {
    StopServer();
}

bool ReceiverEngine::StartServer(uint16_t port, const std::string& save_dir, const std::string& device_name) {
    StopServer();

    m_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen_socket == INVALID_SOCKET) {
        std::cerr << "Failed to create listen socket: " << network::GetSocketErrorString() << std::endl;
        return false;
    }

    int optval = 1;
    setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&optval), sizeof(optval));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(m_listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind listen socket: " << network::GetSocketErrorString() << std::endl;
        CloseSocket(m_listen_socket);
        m_listen_socket = INVALID_SOCKET;
        return false;
    }

    if (listen(m_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Failed to listen: " << network::GetSocketErrorString() << std::endl;
        CloseSocket(m_listen_socket);
        m_listen_socket = INVALID_SOCKET;
        return false;
    }

    m_running = true;
    m_last_activity_seconds = GetCurrentTimeSeconds();
    m_active_transfers = 0;

    try {
        m_thread = std::thread(&ReceiverEngine::ListenLoop, this, save_dir, device_name);
    } catch (const std::exception& e) {
        std::cerr << "Failed to spawn listener thread: " << e.what() << std::endl;
        CloseSocket(m_listen_socket);
        m_listen_socket = INVALID_SOCKET;
        m_running = false;
        return false;
    }
    return true;
}

void ReceiverEngine::StopServer() {
    m_running = false;
    if (m_listen_socket != INVALID_SOCKET) {
        CloseSocket(m_listen_socket);
        m_listen_socket = INVALID_SOCKET;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ReceiverEngine::ListenLoop(const std::string& save_dir, const std::string& device_name) {
    while (m_running) {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);

        SocketType client_socket = accept(m_listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
        if (client_socket == INVALID_SOCKET) {
            if (m_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            continue;
        }

        m_active_transfers++;
        m_last_activity_seconds = GetCurrentTimeSeconds();

        char* ip_str = inet_ntoa(client_addr.sin_addr);
        std::string client_ip = ip_str ? ip_str : "unknown";

        // TCP Socket Tuning — Increase kernel receive buffer for high-throughput reads
        {
            int rcv_buf = 4 * 1024 * 1024;
            setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF,
                       reinterpret_cast<const char*>(&rcv_buf), sizeof(rcv_buf));
        }

        // Process sequentially (eliminates separate worker thread pools and mutex locks)
        HandleClient(client_socket, client_ip, save_dir, device_name);

        m_last_activity_seconds = GetCurrentTimeSeconds();
        m_active_transfers--;
    }
}

void ReceiverEngine::HandleClient(SocketType s, const std::string& client_ip, const std::string& save_dir, const std::string& device_name) {
    (void)device_name;

    // Adaptive Chunk Size: Respond to PING packets before session starts.
    // Sender measures RTT using these pings to pick optimal chunk size.
    network::PacketType type;
    std::vector<uint8_t> payload;
    while (true) {
        if (!network::ReceivePacket(s, type, payload)) {
            std::cerr << ui::Red() << "❌ Reception failed: Connection lost during session startup." << ui::Reset() << std::endl;
            CloseSocket(s);
            return;
        }
        if (type == network::PacketType::PING) {
            // Echo the same payload back as PONG immediately
            network::SendPacket(s, network::PacketType::PONG, payload);
            continue; // Wait for next packet (more PINGs or SESSION_START_REQ)
        }
        if (type == network::PacketType::SESSION_START_REQ) {
            break; // Got the real session request, proceed
        }
        // Unexpected packet type during startup
        CloseSocket(s);
        return;
    }

    if (payload.size() < 20) { // 4+8+4+2+... minimum with chunk_size field
        CloseSocket(s);
        return;
    }

    // Parse Payload — now includes negotiated chunk size from sender
    const uint8_t* ptr = payload.data();
    uint32_t files_count = network::ReadUint32(ptr); ptr += 4;
    uint64_t total_size  = network::ReadUint64(ptr); ptr += 8;
    uint32_t negotiated_chunk_size = network::ReadUint32(ptr); ptr += 4; // New: adaptive chunk size

    // Clamp to sane bounds [256 KB, 16 MB]
    if (negotiated_chunk_size < 256 * 1024)       negotiated_chunk_size = 256 * 1024;
    if (negotiated_chunk_size > 16 * 1024 * 1024) negotiated_chunk_size = 16 * 1024 * 1024;

    uint16_t root_len = (ptr[0] << 8) | ptr[1]; ptr += 2;
    std::string root_name(reinterpret_cast<const char*>(ptr), root_len); ptr += root_len;

    uint16_t sender_len = (ptr[0] << 8) | ptr[1]; ptr += 2;
    std::string sender_name(reinterpret_cast<const char*>(ptr), sender_len);

    // Prompt User
    bool accepted = false;
    {
        std::cout << "\n──────────────────────────────────────────────────" << std::endl;
        std::cout << ui::Yellow() << "🔔 Incoming from: " << sender_name << " (" << client_ip << ")\n" << ui::Reset() << std::endl;
        double size_mb = static_cast<double>(total_size) / (1024.0 * 1024.0);
        std::cout << "   Transfer: " << root_name << " (" << std::fixed << std::setprecision(1) << size_mb << " MB, " << files_count << " files)" << std::endl;
        std::cout << "   Accept? (Y/n) [Auto-decline in 30s]: " << std::flush;

        int timeout_seconds = 30;
        int elapsed_ms = 0;
        std::string response = "";
        
        while (elapsed_ms < timeout_seconds * 1000) {
            // Check if peer disconnected
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(s, &read_fds);
            timeval tv = {0, 0};
            int activity = select(static_cast<int>(s + 1), &read_fds, NULL, NULL, &tv);
            if (activity > 0) {
                char dummy;
                int bytes = recv(s, &dummy, 1, MSG_PEEK);
                if (bytes <= 0) {
                    std::cout << "\n" << ui::Red() << "❌ Sender cancelled or lost connection." << ui::Reset() << std::endl;
                    break;
                }
            }

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
                } else if (ch >= 32 && ch <= 126) {
                    response += static_cast<char>(ch);
                    std::cout << static_cast<char>(ch) << std::flush;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            elapsed_ms += 50;
        }

        if (elapsed_ms >= timeout_seconds * 1000) {
            std::cout << "\n" << ui::Yellow() << "⏰ Acceptance timed out. Automatically declining..." << ui::Reset() << std::endl;
            accepted = false;
        } else {
            // Ensure the socket is still open before setting accepted = true
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(s, &read_fds);
            timeval tv = {0, 0};
            int activity = select(static_cast<int>(s + 1), &read_fds, NULL, NULL, &tv);
            bool socket_ok = true;
            if (activity > 0) {
                char dummy;
                if (recv(s, &dummy, 1, MSG_PEEK) <= 0) {
                    socket_ok = false;
                }
            }

            if (socket_ok) {
                if (response.empty() || response[0] == 'y' || response[0] == 'Y') {
                    accepted = true;
                }
            }
        }
    }

    if (!accepted) {
        uint16_t name_len = static_cast<uint16_t>(device_name.size());
        std::vector<uint8_t> resp_payload(3 + name_len);
        resp_payload[0] = 0; // decline
        resp_payload[1] = (name_len >> 8) & 0xFF;
        resp_payload[2] = name_len & 0xFF;
        std::memcpy(resp_payload.data() + 3, device_name.data(), name_len);

        network::SendPacket(s, network::PacketType::SESSION_START_RESP, resp_payload);
        CloseSocket(s);
        std::cout << ui::Yellow() << "❌ Rejected incoming connection." << ui::Reset() << std::endl;
        std::cout << "\n📡 Still listening... Waiting for sender (Auto-exit on 2.5 min inactivity, Press Ctrl+C to exit)\n" << std::endl;
        return;
    }

    // Send accept response
    uint16_t name_len = static_cast<uint16_t>(device_name.size());
    std::vector<uint8_t> start_resp_payload(7 + name_len, 0);
    start_resp_payload[0] = 1; // 1 = accept
    start_resp_payload[1] = (name_len >> 8) & 0xFF;
    start_resp_payload[2] = name_len & 0xFF;
    std::memcpy(start_resp_payload.data() + 3, device_name.data(), name_len);

    if (!network::SendPacket(s, network::PacketType::SESSION_START_RESP, start_resp_payload)) {
        CloseSocket(s);
        return;
    }

    // Show clear UI message immediately after accept so receiver knows what's happening
    std::cout << "\n" << ui::Yellow() << "⏳ Transfer accepted. Waiting for sender to start..." << ui::Reset() << std::endl;

    std::vector<ReceivedFileMeta> files;
    uint64_t bytes_received = 0;
    uint32_t files_completed = 0;
    ui::ProgressBar progress;
    bool progress_started = false;
    bool session_completed = false;

    crypto::SHA256 active_file_sha;
    uint32_t active_file_id = 0xFFFFFFFF;
    uint64_t active_file_start_offset = 0;
    std::string active_file_computed_hash; // Stores final hash until FILE_HASH_UPDATE arrives

    // Keep one file stream open across all chunks of the same file.
    // This eliminates open/close kernel syscall overhead for each chunk.
    std::ofstream active_out;
    std::string active_out_path;

    // Loop for receiving packets
    while (true) {
        network::PacketType p_type;
        std::vector<uint8_t> p_payload;
        if (!network::ReceivePacket(s, p_type, p_payload)) {
            break;
        }

        if (p_type == network::PacketType::SESSION_END) {
            session_completed = true;
            break;
        }

        // Streaming Hash: Receive final hash from sender and do validation now.
        // This replaces the old "validate on last chunk" approach.
        // Sender computes hash inline while sending, then sends FILE_HASH_UPDATE after all chunks.
        if (p_type == network::PacketType::FILE_HASH_UPDATE) {
            if (p_payload.size() >= 68) {
                uint32_t file_id = network::ReadUint32(p_payload.data());
                std::string sender_hash(reinterpret_cast<const char*>(p_payload.data() + 4), 64);

                uint8_t val_status = 1; // default: pass
                if (file_id < files.size()) {
                    // Compare sender's computed hash with our computed hash
                    if (active_file_computed_hash != sender_hash) {
                        // Validation failed — delete partial file, signal retry
                        if (active_out.is_open()) { active_out.close(); }
                        std::filesystem::remove(active_out_path);
                        std::cout << "\n" << ui::Yellow() << "⚠️  Validation failed for " << files[file_id].relative_path 
                                  << " (Hash mismatch). Deleting corrupt file and waiting for retry..." << ui::Reset() << std::endl;
                        active_file_id = 0xFFFFFFFF;
                        active_file_computed_hash.clear();
                        val_status = 2;
                        if (progress_started) {
                            uint64_t bytes_this_file = files[file_id].total_size - active_file_start_offset;
                            if (bytes_received >= bytes_this_file) bytes_received -= bytes_this_file;
                            else bytes_received = 0;
                            progress.Update(bytes_received, files_completed);
                        }
                    } else {
                        // Hash matched — close file cleanly, count it done
                        if (active_out.is_open()) { active_out.flush(); active_out.close(); }
                        active_file_computed_hash.clear();
                        files_completed++;
                        progress.Update(bytes_received, files_completed);
                    }
                }

                // Send validation result ACK back to sender
                std::vector<uint8_t> val_ack(13);
                network::WriteUint32(val_ack.data(), file_id);
                network::WriteUint64(val_ack.data() + 4, 0);
                val_ack[12] = val_status;
                network::SendPacket(s, network::PacketType::CHUNK_ACK, val_ack);
            }
            continue;
        }

        if (p_type == network::PacketType::FILE_METADATA) {
            if (p_payload.size() < 78) continue;
            uint32_t file_id = network::ReadUint32(p_payload.data());
            uint64_t file_size = network::ReadUint64(p_payload.data() + 4);
            std::string expected_hash(reinterpret_cast<const char*>(p_payload.data() + 12), 64);
            uint16_t path_len = (p_payload[76] << 8) | p_payload[77];
            std::string path(reinterpret_cast<const char*>(p_payload.data() + 78), path_len);

            if (file_id >= files.size()) {
                files.resize(file_id + 1);
            }
            files[file_id] = {file_id, file_size, path, expected_hash};

            // Check if file exists and get size for resuming
            std::string full_path = fileio::NormalizePath(save_dir + "/" + path);
            uint64_t local_size = GetLocalFileSize(full_path);
            
            if (local_size > file_size) {
                local_size = 0; // Overwrite if corrupted
            }
            bytes_received += local_size;

            // Respond with ACK containing the current local size (as resume offset)
            std::vector<uint8_t> ack_payload(13);
            network::WriteUint32(ack_payload.data(), file_id);
            network::WriteUint64(ack_payload.data() + 4, local_size);
            ack_payload[12] = 1; // Success

            network::SendPacket(s, network::PacketType::CHUNK_ACK, ack_payload);
        }
        else if (p_type == network::PacketType::FILE_CHUNK) {
            if (p_payload.size() < 16) continue;
            uint32_t file_id = network::ReadUint32(p_payload.data());
            uint64_t offset = network::ReadUint64(p_payload.data() + 4);
            uint32_t chunk_size = network::ReadUint32(p_payload.data() + 12);
            const uint8_t* chunk_data = p_payload.data() + 16;

            if (file_id >= files.size()) continue;
            const auto& file_meta = files[file_id];

            // If we are starting a new file or starting a retry, open new stream
            if (active_file_id != file_id || offset == 0) {
                // Close previous file stream if open
                if (active_out.is_open()) {
                    active_out.flush();
                    active_out.close();
                }
                active_file_id = file_id;
                active_file_start_offset = offset;
                active_out_path = fileio::NormalizePath(save_dir + "/" + file_meta.relative_path);

                std::string dir = fileio::GetDirectoryOfPath(active_out_path);
                if (!dir.empty()) {
                    fileio::CreateDirectoryRecursive(dir);
                }

                // Open file once for the entire file's transfer
                active_out.open(active_out_path, std::ios::binary | std::ios::in | std::ios::out);
                if (!active_out.is_open()) {
                    active_out.open(active_out_path, std::ios::binary | std::ios::out);
                    if (active_out.is_open()) {
                        active_out.close();
                        active_out.open(active_out_path, std::ios::binary | std::ios::in | std::ios::out);
                    }
                }
                InitializeHashForResume(active_file_sha, active_out_path, offset);
            }

            // Write chunk using persistent file handle (no open/close per chunk)
            uint8_t status = 0;
            if (active_out.is_open()) {
                active_out.seekp(offset, std::ios::beg);
                active_out.write(reinterpret_cast<const char*>(chunk_data), chunk_size);
                status = 1;

                // Update running hash context (free — data already in memory)
                active_file_sha.Update(chunk_data, chunk_size);

                // On last chunk: finalize hash and store it.
                // Do NOT validate yet — wait for FILE_HASH_UPDATE from sender.
                if (offset + chunk_size >= file_meta.total_size) {
                    active_file_computed_hash = active_file_sha.FinalHex();
                    active_out.flush(); // Flush file to disk on completion
                }
            }

            // Send write-confirm ACK (not validation — that happens on FILE_HASH_UPDATE)
            std::vector<uint8_t> ack_payload(13);
            network::WriteUint32(ack_payload.data(), file_id);
            network::WriteUint64(ack_payload.data() + 4, offset);
            ack_payload[12] = status;
            network::SendPacket(s, network::PacketType::CHUNK_ACK, ack_payload);

            if (status == 1) {
                if (!progress_started) {
                    std::cout << "\n──────────────────────────────────────────────────" << std::endl;
                    std::cout << ui::Bold() << "📦 Receiving: " << root_name << " (Press Ctrl+C to abort)\n" << ui::Reset() << std::endl;
                    progress.Start(total_size, files_count, bytes_received);
                    progress_started = true;
                }
                bytes_received += chunk_size;
                progress.Update(bytes_received, files_completed);
            }
        }
    }

    CloseSocket(s);

    if (progress_started) {
        progress.Done();
    }

    if (session_completed) {
        std::cout << "\n" << ui::Green() << "💾 Saved to: " << fileio::NormalizePath(save_dir + "/" + root_name) << ui::Reset() << std::endl;
        std::cout << ui::Green() << "✅ Complete! Received " << progress.FormatBytes(bytes_received) << " successfully." << ui::Reset() << std::endl;
        std::cout << "──────────────────────────────────────────────────\n" << std::endl;
    } else {
        std::cout << "\n" << ui::Red() << "❌ Error: Transfer aborted. Connection with sender lost." << ui::Reset() << std::endl;
        if (progress_started) {
            std::cout << ui::Yellow() << "⚠️  Partially received: " << progress.FormatBytes(bytes_received) << " of " << progress.FormatBytes(total_size) << ui::Reset() << std::endl;
            std::cout << "──────────────────────────────────────────────────\n" << std::endl;
        } else {
            std::cout << ui::Yellow() << "⚠️  No files received." << ui::Reset() << std::endl;
            std::cout << "──────────────────────────────────────────────────\n" << std::endl;
        }
    }
    std::cout << "📡 Still listening... Waiting for next sender (Auto-exit on 2.5 min inactivity) [Press Ctrl+C to exit]\n" << std::endl;
}

} // namespace core
