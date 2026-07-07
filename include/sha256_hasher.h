#ifndef SHA256_HASHER_H
#define SHA256_HASHER_H

#include <cstdint>
#include <string>

namespace crypto {

class SHA256 {
public:
    SHA256();
    void Init();
    void Update(const uint8_t* data, size_t len);
    void Final(uint8_t hash[32]);
    std::string FinalHex();

private:
    uint32_t m_state[8];
    uint8_t m_buffer[64];
    uint64_t m_len;

    static uint32_t RoRight(uint32_t value, uint32_t count);
    void Transform();
};

std::string CalculateSHA256(const uint8_t *data, size_t size);
std::string CalculateSHA256(const std::string &str);
std::string CalculateFileSHA256(const std::string &filepath);

} // namespace crypto

#endif // SHA256_HASHER_H
