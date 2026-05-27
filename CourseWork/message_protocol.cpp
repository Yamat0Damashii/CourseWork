#include "message_protocol.h"

#include <sstream>

#include "util.h"

std::string serializeFragment(const Fragment& f) {
    std::stringstream ss;
    ss << "DMSG|1|" << f.messageId << "|" << f.index << "|" << f.total << "|"
       << f.payload.size() << "|" << f.fragmentCrc << "|" << f.messageCrc << "|"
       << util::hexEncode(f.payload);
    return ss.str();
}

bool parseFragment(const std::string& text, Fragment& f) {
    auto parts = util::split(text, '|');
    if (parts.size() != 9 || parts[0] != "DMSG" || parts[1] != "1") return false;
    try {
        f.messageId = parts[2];
        f.index = static_cast<size_t>(std::stoull(parts[3]));
        f.total = static_cast<size_t>(std::stoull(parts[4]));
        size_t payloadSize = static_cast<size_t>(std::stoull(parts[5]));
        f.fragmentCrc = static_cast<uint32_t>(std::stoul(parts[6]));
        f.messageCrc = static_cast<uint32_t>(std::stoul(parts[7]));
        f.payload = util::hexDecode(parts[8]);
        return f.payload.size() == payloadSize && util::crc32(f.payload) == f.fragmentCrc && f.index < f.total;
    } catch (...) {
        return false;
    }
}
