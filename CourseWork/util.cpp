#include "util.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace util {
std::string hexEncode(const std::string& input) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(input.size() * 2);
    for (unsigned char c : input) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 15]);
    }
    return out;
}

namespace {
int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}

std::string hexDecode(const std::string& input) {
    if (input.size() % 2 != 0) throw std::runtime_error("invalid hex length");
    std::string out;
    out.reserve(input.size() / 2);
    for (size_t i = 0; i < input.size(); i += 2) {
        int hi = hexNibble(input[i]);
        int lo = hexNibble(input[i + 1]);
        if (hi < 0 || lo < 0) throw std::runtime_error("invalid hex data");
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> parts;
    std::string item;
    std::stringstream ss(s);
    while (std::getline(ss, item, delimiter)) parts.push_back(item);
    if (!s.empty() && s.back() == delimiter) parts.emplace_back();
    return parts;
}

std::string trimLeft(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
    return s;
}

uint64_t fnv1a64(const std::string& text) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

std::string hex64(uint64_t value) {
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << value;
    return ss.str();
}

std::string randomHex128() {
    std::random_device rd;
    std::mt19937_64 gen((static_cast<uint64_t>(rd()) << 32) ^ rd());
    return hex64(gen()) + hex64(gen());
}

uint32_t crc32(const std::string& data) {
    static uint32_t table[256] = {};
    static bool ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }

    uint32_t c = 0xffffffffu;
    for (unsigned char b : data) c = table[(c ^ b) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}
