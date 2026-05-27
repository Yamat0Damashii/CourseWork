#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "endpoint.h"
#include "udp_socket.h"

class MiniDhtNode {
public:
    MiniDhtNode(std::string nodeName, uint16_t port);

    void addPeer(const Endpoint& endpoint);
    void put(const std::string& key, const std::string& value);
    std::vector<std::string> get(const std::string& key, int waitMs = 700);
    void pump();
    void printStatus() const;

private:
    void send(const Endpoint& endpoint, const std::string& payload);
    void storeLocal(const std::string& key, const std::string& value);

    std::string nodeName_;
    std::string nodeId_;
    UdpSocket socket_;
    std::map<std::string, Endpoint> peers_;
    std::unordered_map<std::string, std::vector<std::string>> storage_;
};
