#include "Server.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    try {
        asio::io_context io;
        CServer          server(io);

        std::cout << "[Server] Running. Ctrl+C to stop.\n";
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& error) {
        std::cerr << "[Server] fatal: " << error.what() << '\n';
        return 1;
    }
}
