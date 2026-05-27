#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>

#include <cstdint>
#include <string>

#include "endpoint.h"

class WinsockRuntime {
public:
    WinsockRuntime();
    ~WinsockRuntime();

    WinsockRuntime(const WinsockRuntime&) = delete;
    WinsockRuntime& operator=(const WinsockRuntime&) = delete;
};

class UdpSocket {
public:
    explicit UdpSocket(uint16_t port);
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    void sendTo(const Endpoint& endpoint, const std::string& payload);
    bool receive(std::string& payload, Endpoint& from);

private:
    SOCKET socket_ = INVALID_SOCKET;
};
