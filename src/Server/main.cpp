#include "Server/Server.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    try {
        asio::io_context io;
        Net::CServer     srv(io);
        srv.start(Net::DefaultTcpPort, Net::DefaultUdpPort);

        std::cout << "\n[Server] Running. Ctrl+C to stop.\n";

        auto last = std::chrono::steady_clock::now();
        while (true) {
            io.poll();
            auto now = std::chrono::steady_clock::now();
            if (now - last > std::chrono::milliseconds(60)) {
                srv.broadcast();
                last = now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "[Server] fatal: " << e.what() << '\n';
        return 1;
    }
}
