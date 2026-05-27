#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "dht_messenger.h"
#include "endpoint.h"
#include "mini_dht_node.h"
#include "udp_socket.h"
#include "util.h"

using namespace std;

namespace {
void printHelp() {
    cout << "Commands:\n"
         << "  send <recipient> <text>  send a short message through the DHT\n"
         << "  poll                     check inbox once\n"
         << "  listen                   poll inbox until Ctrl+C\n"
         << "  peers                    show DHT status\n"
         << "  help                     show commands\n"
         << "  quit                     exit\n";
}

struct AppConfig {
    string name = "alice";
    uint16_t port = 7001;
    vector<Endpoint> bootstrap;
};

AppConfig parseArgs(int argc, char* argv[]) {
    AppConfig config;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--name" && i + 1 < argc) {
            config.name = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            int parsedPort = stoi(argv[++i]);
            if (parsedPort <= 0 || parsedPort > 65535) throw runtime_error("invalid port");
            config.port = static_cast<uint16_t>(parsedPort);
        } else if (arg == "--peer" && i + 1 < argc) {
            Endpoint endpoint;
            if (!parseEndpoint(argv[++i], endpoint)) throw runtime_error("invalid peer endpoint");
            config.bootstrap.push_back(endpoint);
        } else if (arg == "--help") {
            cout << "Usage: CourseWork.exe --name alice --port 7001 [--peer 127.0.0.1:7002]\n";
            printHelp();
            exit(0);
        }
    }
    return config;
}

void runCommandLoop(const string& name, MiniDhtNode& dht, DhtMessenger& messenger) {
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
}
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    try {
        AppConfig config = parseArgs(argc, argv);
        WinsockRuntime winsock;
        MiniDhtNode dht(config.name, config.port);
        DhtMessenger messenger(dht);
        for (const Endpoint& peer : config.bootstrap) dht.addPeer(peer);

        cout << "DHT messenger node started as '" << config.name << "' on UDP port " << config.port << ".\n";
        cout << "DHT model: Kademlia/Mainline-style distributed key-value storage for small values.\n";
        printHelp();

        runCommandLoop(config.name, dht, messenger);
    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
