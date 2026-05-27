#pragma once

#include <cstddef>
#include <set>
#include <string>

#include "mini_dht_node.h"

class DhtMessenger {
public:
    explicit DhtMessenger(MiniDhtNode& dht);

    void sendMessage(const std::string& recipient, const std::string& text);
    void pollInbox(const std::string& myName);

private:
    struct AssembleResult {
        bool ok = false;
        std::string text;
        std::string reason;
    };

    static std::string inboxKey(const std::string& name);
    static std::string fragmentKey(const std::string& messageId, size_t index);

    AssembleResult tryAssemble(const std::string& id);

    MiniDhtNode& dht_;
    std::set<std::string> seenMessages_;
};
