#include "Master/MasterServer.hpp"
#include "Server/Server.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

    struct SServerOptions {
        std::string   masterAddress;
        std::uint16_t masterPort        = Net::Control::DefaultMasterPort;
        std::string   serverName        = "server1";
        std::string   advertisedAddress = "127.0.0.1";
        std::uint16_t tcpPort           = Net::DefaultTcpPort;
        std::uint16_t udpPort           = Net::DefaultUdpPort;
    };

    [[nodiscard]] std::uint16_t parsePort(const char* value) {
        const auto port = std::strtoul(value, nullptr, 10);
        if (port == 0 || port > 65535)
            throw std::runtime_error("invalid port");
        return static_cast<std::uint16_t>(port);
    }

    [[nodiscard]] SServerOptions parseServerOptions(int argc, char* argv[]) {
        SServerOptions options;
        options.masterAddress = argv[1];
        for (int index = 2; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--name" && index + 1 < argc) {
                options.serverName = argv[++index];
            } else if (argument == "--advertise" && index + 1 < argc) {
                options.advertisedAddress = argv[++index];
            } else if (argument == "--tcp-port" && index + 1 < argc) {
                options.tcpPort = parsePort(argv[++index]);
            } else if (argument == "--udp-port" && index + 1 < argc) {
                options.udpPort = parsePort(argv[++index]);
            } else if (argument == "--master-port" && index + 1 < argc) {
                options.masterPort = parsePort(argv[++index]);
            } else {
                throw std::runtime_error("unknown option: " + argument);
            }
        }
        return options;
    }

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            asio::io_context io;
            CMasterServer    master(io);
            master.start();
            std::cout << "[Master] Running. Ctrl+C to stop.\n";
            while (true)
                std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        const auto       options = parseServerOptions(argc, argv);
        asio::io_context io;
        CServer          server(io, options.tcpPort, options.udpPort,
                                SMasterConfig{
                                    .masterAddress     = options.masterAddress,
                                    .masterPort        = options.masterPort,
                                    .serverName        = options.serverName,
                                    .advertisedAddress = options.advertisedAddress,
                                    .maxPlayers        = Room::MaxConnections,
                       });

        std::cout << "[Server] Running. Ctrl+C to stop.\n";
        while (true)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (const std::exception& error) {
        std::cerr << "[Server] fatal: " << error.what() << '\n';
        return 1;
    }
}
