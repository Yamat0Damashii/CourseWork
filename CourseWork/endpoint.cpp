#include "endpoint.h"

#include <stdexcept>

std::string Endpoint::key() const {
    return host + ":" + std::to_string(port);
}

bool parseEndpoint(const std::string& value, Endpoint& endpoint) {
    size_t pos = value.rfind(':');
    if (pos == std::string::npos) return false;
    endpoint.host = value.substr(0, pos);
    int parsedPort = std::stoi(value.substr(pos + 1));
    if (parsedPort <= 0 || parsedPort > 65535) return false;
    endpoint.port = static_cast<uint16_t>(parsedPort);
    return true;
}
