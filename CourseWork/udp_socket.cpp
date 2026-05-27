#include "udp_socket.h"

#include <ws2tcpip.h>

#include <stdexcept>

WinsockRuntime::WinsockRuntime() {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
}

WinsockRuntime::~WinsockRuntime() {
    WSACleanup();
}

UdpSocket::UdpSocket(uint16_t port) {
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) throw std::runtime_error("cannot create UDP socket");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(socket_);
        throw std::runtime_error("cannot bind UDP port " + std::to_string(port));
    }

    u_long nonBlocking = 1;
    ioctlsocket(socket_, FIONBIO, &nonBlocking);
}

UdpSocket::~UdpSocket() {
    if (socket_ != INVALID_SOCKET) closesocket(socket_);
}

void UdpSocket::sendTo(const Endpoint& endpoint, const std::string& payload) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* result = nullptr;
    std::string port = std::to_string(endpoint.port);
    if (getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &result) != 0) return;
    sendto(socket_, payload.c_str(), static_cast<int>(payload.size()), 0, result->ai_addr, static_cast<int>(result->ai_addrlen));
    freeaddrinfo(result);
}

bool UdpSocket::receive(std::string& payload, Endpoint& from) {
    char buffer[4096];
    sockaddr_in addr{};
    int addrLen = sizeof(addr);
    int n = recvfrom(socket_, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&addr), &addrLen);
    if (n == SOCKET_ERROR) return false;

    buffer[n] = '\0';
    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    from.host = ip;
    from.port = ntohs(addr.sin_port);
    payload.assign(buffer, buffer + n);
    return true;
}
