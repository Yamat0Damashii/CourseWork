#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std;

namespace util {
string hexEncode(const string& input) {
    static const char* digits = "0123456789abcdef";
    string out;
    out.reserve(input.size() * 2);
    for (unsigned char c : input) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 15]);
    }
    return out;
}

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

string hexDecode(const string& input) {
    if (input.size() % 2 != 0) throw runtime_error("invalid hex length");
    string out;
    out.reserve(input.size() / 2);
    for (size_t i = 0; i < input.size(); i += 2) {
        int hi = hexNibble(input[i]);
        int lo = hexNibble(input[i + 1]);
        if (hi < 0 || lo < 0) throw runtime_error("invalid hex data");
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

vector<string> split(const string& s, char delimiter) {
    vector<string> parts;
    string item;
    stringstream ss(s);
    while (getline(ss, item, delimiter)) parts.push_back(item);
    if (!s.empty() && s.back() == delimiter) parts.emplace_back();
    return parts;
}

string trimLeft(string s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char c) { return !isspace(c); }));
    return s;
}

uint64_t fnv1a64(const string& text) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

string hex64(uint64_t value) {
    stringstream ss;
    ss << hex << setw(16) << setfill('0') << value;
    return ss.str();
}

string randomHex128() {
    random_device rd;
    mt19937_64 gen((static_cast<uint64_t>(rd()) << 32) ^ rd());
    return hex64(gen()) + hex64(gen());
}

uint32_t crc32(const string& data) {
    static uint32_t table[256] = {};
    static bool ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }

    uint32_t c = 0xffffffffu;
    for (unsigned char b : data) c = table[(c ^ b) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return s;
}
}

struct Endpoint {
    string host;
    uint16_t port = 0;

    string key() const {
        return host + ":" + to_string(port);
    }
};

bool parseEndpoint(const string& value, Endpoint& endpoint) {
    size_t pos = value.rfind(':');
    if (pos == string::npos) return false;
    endpoint.host = value.substr(0, pos);
    int port = stoi(value.substr(pos + 1));
    if (port <= 0 || port > 65535) return false;
    endpoint.port = static_cast<uint16_t>(port);
    return true;
}

class WinsockRuntime {
public:
    WinsockRuntime() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw runtime_error("WSAStartup failed");
    }

    ~WinsockRuntime() {
        WSACleanup();
    }
};

class UdpSocket {
public:
    explicit UdpSocket(uint16_t port) {
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) throw runtime_error("cannot create UDP socket");

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        if (bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            closesocket(socket_);
            throw runtime_error("cannot bind UDP port " + to_string(port));
        }

        u_long nonBlocking = 1;
        ioctlsocket(socket_, FIONBIO, &nonBlocking);
    }

    ~UdpSocket() {
        if (socket_ != INVALID_SOCKET) closesocket(socket_);
    }

    void sendTo(const Endpoint& endpoint, const string& payload) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        addrinfo* result = nullptr;
        string port = to_string(endpoint.port);
        if (getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &result) != 0) return;
        sendto(socket_, payload.c_str(), static_cast<int>(payload.size()), 0, result->ai_addr, static_cast<int>(result->ai_addrlen));
        freeaddrinfo(result);
    }

    bool receive(string& payload, Endpoint& from) {
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

private:
    SOCKET socket_ = INVALID_SOCKET;
};

class MiniDhtNode {
public:
    MiniDhtNode(string nodeName, uint16_t port) : nodeName_(move(nodeName)), nodeId_(util::hex64(util::fnv1a64(nodeName_))), socket_(port) {}

    void addPeer(const Endpoint& endpoint) {
        peers_[endpoint.key()] = endpoint;
        send(endpoint, "PING\t" + nodeId_);
    }

    void put(const string& key, const string& value) {
        storeLocal(key, value);
        string packet = "STORE\t" + util::hexEncode(key) + "\t" + util::hexEncode(value);
        for (const auto& [_, peer] : peers_) send(peer, packet);
    }

    vector<string> get(const string& key, int waitMs = 700) {
        string packet = "FIND\t" + util::hexEncode(key);
        for (const auto& [_, peer] : peers_) send(peer, packet);

        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(waitMs);
        while (chrono::steady_clock::now() < deadline) {
            pump();
            this_thread::sleep_for(chrono::milliseconds(15));
        }

        auto it = storage_.find(key);
        if (it == storage_.end()) return {};
        return it->second;
    }

    void pump() {
        string payload;
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
                    string key = util::hexDecode(parts[1]);
                    string value = util::hexDecode(parts[2]);
                    storeLocal(key, value);
                    send(from, "STORED\t" + parts[1]);
                } catch (...) {
                }
            } else if (parts[0] == "FIND" && parts.size() >= 2) {
                try {
                    string key = util::hexDecode(parts[1]);
                    auto it = storage_.find(key);
                    if (it != storage_.end()) {
                        for (const string& value : it->second) {
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

    void printStatus() const {
        cout << "Node id: " << nodeId_ << "\n";
        cout << "Known peers: " << peers_.size() << "\n";
        for (const auto& [_, peer] : peers_) cout << "  " << peer.key() << "\n";
        cout << "Local DHT keys: " << storage_.size() << "\n";
    }

private:
    void send(const Endpoint& endpoint, const string& payload) {
        socket_.sendTo(endpoint, payload);
    }

    void storeLocal(const string& key, const string& value) {
        auto& values = storage_[key];
        if (find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
    }

    string nodeName_;
    string nodeId_;
    UdpSocket socket_;
    map<string, Endpoint> peers_;
    unordered_map<string, vector<string>> storage_;
};

struct Fragment {
    string messageId;
    size_t index = 0;
    size_t total = 0;
    uint32_t fragmentCrc = 0;
    uint32_t messageCrc = 0;
    string payload;
};

string serializeFragment(const Fragment& f) {
    stringstream ss;
    ss << "DMSG|1|" << f.messageId << "|" << f.index << "|" << f.total << "|"
       << f.payload.size() << "|" << f.fragmentCrc << "|" << f.messageCrc << "|"
       << util::hexEncode(f.payload);
    return ss.str();
}

bool parseFragment(const string& text, Fragment& f) {
    auto parts = util::split(text, '|');
    if (parts.size() != 9 || parts[0] != "DMSG" || parts[1] != "1") return false;
    try {
        f.messageId = parts[2];
        f.index = static_cast<size_t>(stoull(parts[3]));
        f.total = static_cast<size_t>(stoull(parts[4]));
        size_t payloadSize = static_cast<size_t>(stoull(parts[5]));
        f.fragmentCrc = static_cast<uint32_t>(stoul(parts[6]));
        f.messageCrc = static_cast<uint32_t>(stoul(parts[7]));
        f.payload = util::hexDecode(parts[8]);
        return f.payload.size() == payloadSize && util::crc32(f.payload) == f.fragmentCrc && f.index < f.total;
    } catch (...) {
        return false;
    }
}

class DhtMessenger {
public:
    explicit DhtMessenger(MiniDhtNode& dht) : dht_(dht) {}

    void sendMessage(const string& recipient, const string& text) {
        constexpr size_t maxPayload = 360;
        string messageId = util::randomHex128();
        uint32_t messageCrc = util::crc32(text);
        size_t total = max<size_t>(1, (text.size() + maxPayload - 1) / maxPayload);

        for (size_t i = 0; i < total; ++i) {
            string chunk = text.substr(i * maxPayload, maxPayload);
            Fragment f{messageId, i, total, util::crc32(chunk), messageCrc, chunk};
            dht_.put(fragmentKey(messageId, i), serializeFragment(f));
        }

        dht_.put(inboxKey(recipient), messageId);
        cout << "Sent message " << messageId << " to " << recipient << " in " << total << " fragment(s).\n";
    }

    void pollInbox(const string& myName) {
        auto ids = dht_.get(inboxKey(myName));
        if (ids.empty()) {
            cout << "No messages for " << myName << ".\n";
            return;
        }

        bool printed = false;
        for (const string& id : ids) {
            if (seenMessages_.count(id)) continue;
            auto message = tryAssemble(id);
            if (!message.ok) {
                cout << "Message " << id << " is not complete yet (" << message.reason << ").\n";
                continue;
            }

            seenMessages_.insert(id);
            printed = true;
            cout << "\n[" << id << "] " << message.text << "\n\n";
        }

        if (!printed) cout << "No new complete messages.\n";
    }

private:
    struct AssembleResult {
        bool ok = false;
        string text;
        string reason;
    };

    static string inboxKey(const string& name) {
        return "inbox:" + util::hex64(util::fnv1a64(util::toLower(name)));
    }

    static string fragmentKey(const string& messageId, size_t index) {
        return "msg:" + messageId + ":" + to_string(index);
    }

    AssembleResult tryAssemble(const string& id) {
        map<size_t, Fragment> fragments;
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
            for (const string& value : values) {
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

        string text;
        for (size_t i = 0; i < expectedTotal; ++i) text += fragments[i].payload;
        if (util::crc32(text) != expectedMessageCrc) return {false, "", "message CRC check failed"};
        return {true, text, ""};
    }

    MiniDhtNode& dht_;
    set<string> seenMessages_;
};

void printHelp() {
    cout << "Commands:\n"
         << "  send <recipient> <text>  send a short message through the DHT\n"
         << "  poll                     check inbox once\n"
         << "  listen                   poll inbox until Ctrl+C\n"
         << "  peers                    show DHT status\n"
         << "  help                     show commands\n"
         << "  quit                     exit\n";
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    string name = "alice";
    uint16_t port = 7001;
    vector<Endpoint> bootstrap;

    try {
        for (int i = 1; i < argc; ++i) {
            string arg = argv[i];
            if (arg == "--name" && i + 1 < argc) {
                name = argv[++i];
            } else if (arg == "--port" && i + 1 < argc) {
                int p = stoi(argv[++i]);
                if (p <= 0 || p > 65535) throw runtime_error("invalid port");
                port = static_cast<uint16_t>(p);
            } else if (arg == "--peer" && i + 1 < argc) {
                Endpoint endpoint;
                if (!parseEndpoint(argv[++i], endpoint)) throw runtime_error("invalid peer endpoint");
                bootstrap.push_back(endpoint);
            } else if (arg == "--help") {
                cout << "Usage: CourseWork.exe --name alice --port 7001 [--peer 127.0.0.1:7002]\n";
                printHelp();
                return 0;
            }
        }

        WinsockRuntime winsock;
        MiniDhtNode dht(name, port);
        DhtMessenger messenger(dht);
        for (const Endpoint& peer : bootstrap) dht.addPeer(peer);

        cout << "DHT messenger node started as '" << name << "' on UDP port " << port << ".\n";
        cout << "DHT model: Kademlia/Mainline-style distributed key-value storage for small values.\n";
        printHelp();

        string line;
        while (true) {
            dht.pump();
            cout << "> ";
            if (!getline(cin, line)) break;
            dht.pump();

            auto parts = util::split(line, ' ');
            if (parts.empty() || parts[0].empty()) continue;
            string command = parts[0];

            if (command == "quit" || command == "exit") {
                break;
            } else if (command == "help") {
                printHelp();
            } else if (command == "peers") {
                dht.printStatus();
            } else if (command == "poll") {
                messenger.pollInbox(name);
            } else if (command == "listen") {
                cout << "Listening for messages. Press Ctrl+C to stop.\n";
                while (true) {
                    dht.pump();
                    messenger.pollInbox(name);
                    this_thread::sleep_for(chrono::seconds(2));
                }
            } else if (command == "send") {
                if (parts.size() < 3) {
                    cout << "Usage: send <recipient> <text>\n";
                    continue;
                }
                string recipient = parts[1];
                size_t textStart = line.find(recipient);
                textStart = line.find(' ', textStart + recipient.size());
                string text = textStart == string::npos ? "" : util::trimLeft(line.substr(textStart));
                messenger.sendMessage(recipient, text);
            } else {
                cout << "Unknown command. Type help.\n";
            }
        }
    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
