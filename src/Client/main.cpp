#include "Client.hpp"
#include "Shared/ControlProtocol.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

    [[nodiscard]] std::uint16_t parsePort(const char* value) {
        const auto port = std::strtoul(value, nullptr, 10);
        if (port == 0 || port > 65535)
            throw std::runtime_error("invalid port");
        return static_cast<std::uint16_t>(port);
    }

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <master-ip> [--master-port <port>] [--server <name>]\n";
        return 1;
    }

    try {
        const std::string masterAddress = argv[1];
        std::uint16_t     masterPort    = Net::Control::DefaultMasterPort;
        std::string       preferredServer;
        for (int index = 2; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--master-port" && index + 1 < argc) {
                masterPort = parsePort(argv[++index]);
            } else if (argument == "--server" && index + 1 < argc) {
                preferredServer = argv[++index];
            } else {
                std::cerr << "Unknown option: " << argument << '\n';
                return 1;
            }
        }

        CClient client(masterAddress, masterPort, preferredServer);
        while (client.running())
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } catch (const std::exception& error) {
        std::cerr << "[Client] fatal: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
