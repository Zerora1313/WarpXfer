#ifndef NETWORK_COMPAT_H
#define NETWORK_COMPAT_H

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET SocketType;
    #define CloseSocket closesocket
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <string.h>
    #include <errno.h>
    typedef int SocketType;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define CloseSocket close
#endif

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

namespace network {

    inline bool InitSockets() {
#ifdef _WIN32
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
        return true;
#endif
    }
    
    inline void CleanupSockets() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    inline std::string GetSocketErrorString() {
#ifdef _WIN32
        int err = WSAGetLastError();
        char* s = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, err,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPSTR)&s, 0, NULL);
        std::string result = "Error code: " + std::to_string(err);
        if (s) {
            result += " (" + std::string(s) + ")";
            LocalFree(s);
        }
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
            result.pop_back();
        }
        return result;
#else
        return std::string(strerror(errno)) + " (code: " + std::to_string(errno) + ")";
#endif
    }
    
    inline bool SetSocketNonBlocking(SocketType s, bool nonblocking) {
#ifdef _WIN32
        u_long mode = nonblocking ? 1 : 0;
        return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
        int flags = fcntl(s, F_GETFL, 0);
        if (flags == -1) return false;
        flags = nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
        return fcntl(s, F_SETFL, flags) == 0;
#endif
    }

    // Packet Protocol Helpers (Merged from protocol)
    enum class PacketType : uint8_t {
        DISCOVER_REQ = 1,
        DISCOVER_RESP = 2,
        SESSION_START_REQ = 3,
        SESSION_START_RESP = 4,
        FILE_METADATA = 5,
        FILE_CHUNK = 6,
        CHUNK_ACK = 7,
        SESSION_END = 8,
        FILE_HASH_UPDATE = 9,  // Sent by sender to update receiver's expected hash after lazy compute
        PING = 10,             // Adaptive chunk size: RTT measurement request
        PONG = 11              // Adaptive chunk size: RTT measurement response
    };

    const uint8_t MAGIC_BYTES[4] = {'W', 'A', 'R', 'P'};

    inline void WriteUint32(uint8_t* buf, uint32_t val) {
        buf[0] = (val >> 24) & 0xFF;
        buf[1] = (val >> 16) & 0xFF;
        buf[2] = (val >> 8) & 0xFF;
        buf[3] = val & 0xFF;
    }

    inline uint32_t ReadUint32(const uint8_t* buf) {
        return ((uint32_t)buf[0] << 24) |
               ((uint32_t)buf[1] << 16) |
               ((uint32_t)buf[2] << 8)  |
               (uint32_t)buf[3];
    }

    inline void WriteUint64(uint8_t* buf, uint64_t val) {
        buf[0] = (val >> 56) & 0xFF;
        buf[1] = (val >> 48) & 0xFF;
        buf[2] = (val >> 40) & 0xFF;
        buf[3] = (val >> 32) & 0xFF;
        buf[4] = (val >> 24) & 0xFF;
        buf[5] = (val >> 16) & 0xFF;
        buf[6] = (val >> 8) & 0xFF;
        buf[7] = val & 0xFF;
    }

    inline uint64_t ReadUint64(const uint8_t* buf) {
        return ((uint64_t)buf[0] << 56) |
               ((uint64_t)buf[1] << 48) |
               ((uint64_t)buf[2] << 40) |
               ((uint64_t)buf[3] << 32) |
               ((uint64_t)buf[4] << 24) |
               ((uint64_t)buf[5] << 16) |
               ((uint64_t)buf[6] << 8)  |
               (uint64_t)buf[7];
    }

    inline bool SendAll(SocketType s, const uint8_t* buf, int size) {
        int total_sent = 0;
        while (total_sent < size) {
            int sent = send(s, reinterpret_cast<const char*>(buf + total_sent), size - total_sent, 0);
            if (sent <= 0) {
                return false;
            }
            total_sent += sent;
        }
        return true;
    }

    inline bool RecvAll(SocketType s, uint8_t* buf, int size) {
        int total_recvd = 0;
        while (total_recvd < size) {
            int recvd = recv(s, reinterpret_cast<char*>(buf + total_recvd), size - total_recvd, 0);
            if (recvd <= 0) {
                return false;
            }
            total_recvd += recvd;
        }
        return true;
    }

    inline bool SendPacket(SocketType s, PacketType type, const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> header_buf(9);
        std::memcpy(&header_buf[0], MAGIC_BYTES, 4);
        header_buf[4] = static_cast<uint8_t>(type);
        WriteUint32(&header_buf[5], static_cast<uint32_t>(payload.size()));

        if (!SendAll(s, header_buf.data(), 9)) {
            return false;
        }

        if (!payload.empty()) {
            if (!SendAll(s, payload.data(), static_cast<int>(payload.size()))) {
                return false;
            }
        }
        return true;
    }

    inline bool ReceivePacket(SocketType s, PacketType& type, std::vector<uint8_t>& payload) {
        uint8_t header_buf[9];
        if (!RecvAll(s, header_buf, 9)) {
            return false;
        }

        if (std::memcmp(header_buf, MAGIC_BYTES, 4) != 0) {
            return false;
        }

        type = static_cast<PacketType>(header_buf[4]);
        uint32_t length = ReadUint32(&header_buf[5]);

        payload.resize(length);
        if (length > 0) {
            if (!RecvAll(s, payload.data(), length)) {
                return false;
            }
        }
        return true;
    }

} // namespace network

#endif // NETWORK_COMPAT_H
