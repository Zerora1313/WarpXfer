#ifndef SHA256_HASHER_H
#define SHA256_HASHER_H

#include <cstdint>
#include <string>

namespace crypto {
std::string CalculateSHA256(const uint8_t *data, size_t size);
std::string CalculateSHA256(const std::string &str);
} // namespace crypto

#endif // SHA256_HASHER_H
