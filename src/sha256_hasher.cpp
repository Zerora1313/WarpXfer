#include "../include/sha256_hasher.h"
#include <iomanip>
#include <sstream>
#include <cstring>
#include <fstream>
#include <vector>

namespace crypto {

SHA256::SHA256() {
    Init();
}

void SHA256::Init() {
    m_state[0] = 0x6a09e667;
    m_state[1] = 0xbb67ae85;
    m_state[2] = 0x3c6ef372;
    m_state[3] = 0xa54ff53a;
    m_state[4] = 0x510e527f;
    m_state[5] = 0x9b05688c;
    m_state[6] = 0x1f83d9ab;
    m_state[7] = 0x5be0cd19;
    m_len = 0;
}

void SHA256::Update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        m_buffer[m_len & 63] = data[i];
        m_len++;
        if ((m_len & 63) == 0) {
            Transform();
        }
    }
}

void SHA256::Final(uint8_t hash[32]) {
    uint64_t total_bits = m_len * 8;
    Update((const uint8_t*)"\x80", 1);
    while ((m_len & 63) != 56) {
        Update((const uint8_t*)"\x00", 1);
    }
    uint8_t len_bytes[8];
    for (int i = 0; i < 8; ++i) {
        len_bytes[i] = (total_bits >> (56 - i * 8)) & 0xFF;
    }
    Update(len_bytes, 8);

    for (int i = 0; i < 8; ++i) {
        hash[i * 4]     = (m_state[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (m_state[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (m_state[i] >> 8)  & 0xFF;
        hash[i * 4 + 3] = m_state[i]        & 0xFF;
    }
}

std::string SHA256::FinalHex() {
    uint8_t hash[32];
    Final(hash);
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

uint32_t SHA256::RoRight(uint32_t value, uint32_t count) {
    return (value >> count) | (value << (32 - count));
}

void SHA256::Transform() {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (m_buffer[i * 4] << 24) | (m_buffer[i * 4 + 1] << 16) |
               (m_buffer[i * 4 + 2] << 8)  | m_buffer[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = RoRight(w[i - 15], 7) ^ RoRight(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = RoRight(w[i - 2], 17) ^ RoRight(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = m_state[0];
    uint32_t b = m_state[1];
    uint32_t c = m_state[2];
    uint32_t d = m_state[3];
    uint32_t e = m_state[4];
    uint32_t f = m_state[5];
    uint32_t g = m_state[6];
    uint32_t h = m_state[7];

    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = RoRight(e, 6) ^ RoRight(e, 11) ^ RoRight(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = RoRight(a, 2) ^ RoRight(a, 13) ^ RoRight(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
    m_state[4] += e;
    m_state[5] += f;
    m_state[6] += g;
    m_state[7] += h;
}

std::string CalculateSHA256(const uint8_t* data, size_t size) {
    SHA256 sha;
    sha.Update(data, size);
    return sha.FinalHex();
}

std::string CalculateSHA256(const std::string& str) {
    return CalculateSHA256(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

std::string CalculateFileSHA256(const std::string& filepath) {
    SHA256 sha;
    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) return "";
    const size_t buf_size = 64 * 1024;
    std::vector<uint8_t> buffer(buf_size);
    while (true) {
        f.read(reinterpret_cast<char*>(buffer.data()), buf_size);
        std::streamsize bytes = f.gcount();
        if (bytes <= 0) break;
        sha.Update(buffer.data(), bytes);
    }
    return sha.FinalHex();
}

} // namespace crypto
