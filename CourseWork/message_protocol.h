#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct Fragment {
    std::string messageId;
    size_t index = 0;
    size_t total = 0;
    uint32_t fragmentCrc = 0;
    uint32_t messageCrc = 0;
    std::string payload;
};

std::string serializeFragment(const Fragment& f);
bool parseFragment(const std::string& text, Fragment& f);
