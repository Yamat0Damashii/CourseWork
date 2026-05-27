#pragma once

#include <cstdint>
#include <string>

struct Endpoint {
    std::string host;
    uint16_t port = 0;

    std::string key() const;
};

bool parseEndpoint(const std::string& value, Endpoint& endpoint);
