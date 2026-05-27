#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace util {
std::string hexEncode(const std::string& input);
std::string hexDecode(const std::string& input);
std::vector<std::string> split(const std::string& s, char delimiter);
std::string trimLeft(std::string s);
uint64_t fnv1a64(const std::string& text);
std::string hex64(uint64_t value);
std::string randomHex128();
uint32_t crc32(const std::string& data);
std::string toLower(std::string s);
}
