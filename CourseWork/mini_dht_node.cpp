#include "mini_dht_node.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

#include "util.h"

MiniDhtNode::MiniDhtNode(std::string nodeName, uint16_t port)
    : nodeName_(std::move(nodeName)), nodeId_(util::hex64(util::fnv1a64(nodeName_))), socket_(port) {}

void MiniDhtNode::addPeer(const Endpoint& endpoint) {
    peers_[endpoint.key()] = endpoint;
    send(endpoint, "PING\t" + nodeId_);
}

void MiniDhtNode::put(const std::string& key, const std::string& value) {
    storeLocal(key, value);
    std::string packet = "STORE\t" + util::hexEncode(key) + "\t" + util::hexEncode(value);
    for (const auto& [_, peer] : peers_) send(peer, packet);
}

std::vector<std::string> MiniDhtNode::get(const std::string& key, int waitMs) {
    std::string packet = "FIND\t" + util::hexEncode(key);
    for (const auto& [_, peer] : peers_) send(peer, packet);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
    while (std::chrono::steady_clock::now() < deadline) {
        pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    auto it = storage_.find(key);
    if (it == storage_.end()) return {};
    return it->second;
}

void MiniDhtNode::pump() {
    std::string payload;
    Endpoint from;
    while (socket_.receive(payload, from)) {
        auto parts = util::split(payload, '\t');
        if (parts.empty()) continue;

        if (parts[0] == "PING" && parts.size() >= 2) {
            peers_[from.key()] = from;
            send(from, "PONG\t" + nodeId_);
        } else if (parts[0] == "PONG" && parts.size() >= 2) {
            peers_[from.key()] = from;
        } else if (parts[0] == "STORE" && parts.size() >= 3) {
            try {
                std::string key = util::hexDecode(parts[1]);
                std::string value = util::hexDecode(parts[2]);
                storeLocal(key, value);
                send(from, "STORED\t" + parts[1]);
            } catch (...) {
            }
        } else if (parts[0] == "FIND" && parts.size() >= 2) {
            try {
                std::string key = util::hexDecode(parts[1]);
                auto it = storage_.find(key);
                if (it != storage_.end()) {
                    for (const std::string& value : it->second) {
                        send(from, "VALUE\t" + parts[1] + "\t" + util::hexEncode(value));
                    }
                }
            } catch (...) {
            }
        } else if (parts[0] == "VALUE" && parts.size() >= 3) {
            try {
                storeLocal(util::hexDecode(parts[1]), util::hexDecode(parts[2]));
            } catch (...) {
            }
        }
    }
}

void MiniDhtNode::printStatus() const {
    std::cout << "Node id: " << nodeId_ << "\n";
    std::cout << "Known peers: " << peers_.size() << "\n";
    for (const auto& [_, peer] : peers_) std::cout << "  " << peer.key() << "\n";
    std::cout << "Local DHT keys: " << storage_.size() << "\n";
}

void MiniDhtNode::send(const Endpoint& endpoint, const std::string& payload) {
    socket_.sendTo(endpoint, payload);
}

void MiniDhtNode::storeLocal(const std::string& key, const std::string& value) {
    auto& values = storage_[key];
    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}
