#include "dht_messenger.h"

#include <algorithm>
#include <iostream>
#include <map>

#include "message_protocol.h"
#include "util.h"

DhtMessenger::DhtMessenger(MiniDhtNode& dht) : dht_(dht) {}

void DhtMessenger::sendMessage(const std::string& recipient, const std::string& text) {
    constexpr size_t maxPayload = 360;
    std::string messageId = util::randomHex128();
    uint32_t messageCrc = util::crc32(text);
    size_t total = std::max<size_t>(1, (text.size() + maxPayload - 1) / maxPayload);

    for (size_t i = 0; i < total; ++i) {
        std::string chunk = text.substr(i * maxPayload, maxPayload);
        Fragment f{messageId, i, total, util::crc32(chunk), messageCrc, chunk};
        dht_.put(fragmentKey(messageId, i), serializeFragment(f));
    }

    dht_.put(inboxKey(recipient), messageId);
    std::cout << "Sent message " << messageId << " to " << recipient << " in " << total << " fragment(s).\n";
}

void DhtMessenger::pollInbox(const std::string& myName) {
    auto ids = dht_.get(inboxKey(myName));
    if (ids.empty()) {
        std::cout << "No messages for " << myName << ".\n";
        return;
    }

    bool printed = false;
    for (const std::string& id : ids) {
        if (seenMessages_.count(id)) continue;
        auto message = tryAssemble(id);
        if (!message.ok) {
            std::cout << "Message " << id << " is not complete yet (" << message.reason << ").\n";
            continue;
        }

        seenMessages_.insert(id);
        printed = true;
        std::cout << "\n[" << id << "] " << message.text << "\n\n";
    }

    if (!printed) std::cout << "No new complete messages.\n";
}

std::string DhtMessenger::inboxKey(const std::string& name) {
    return "inbox:" + util::hex64(util::fnv1a64(util::toLower(name)));
}

std::string DhtMessenger::fragmentKey(const std::string& messageId, size_t index) {
    return "msg:" + messageId + ":" + std::to_string(index);
}

DhtMessenger::AssembleResult DhtMessenger::tryAssemble(const std::string& id) {
    std::map<size_t, Fragment> fragments;
    size_t expectedTotal = 0;
    uint32_t expectedMessageCrc = 0;

    for (size_t i = 0; i < 512; ++i) {
        auto values = dht_.get(fragmentKey(id, i), 250);
        if (values.empty()) {
            if (i == 0) return {false, "", "first fragment is missing"};
            break;
        }

        Fragment f;
        bool found = false;
        for (const std::string& value : values) {
            if (parseFragment(value, f) && f.messageId == id && f.index == i) {
                found = true;
                break;
            }
        }
        if (!found) return {false, "", "fragment CRC check failed"};

        if (i == 0) {
            expectedTotal = f.total;
            expectedMessageCrc = f.messageCrc;
        }
        if (f.total != expectedTotal || f.messageCrc != expectedMessageCrc) return {false, "", "header mismatch"};
        fragments[i] = f;
        if (fragments.size() == expectedTotal) break;
    }

    if (expectedTotal == 0 || fragments.size() != expectedTotal) return {false, "", "not all fragments are available"};

    std::string text;
    for (size_t i = 0; i < expectedTotal; ++i) text += fragments[i].payload;
    if (util::crc32(text) != expectedMessageCrc) return {false, "", "message CRC check failed"};
    return {true, text, ""};
}
