#include "Client.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <server-ip>\n";
        return 1;
    }

    try {
        CClient client(argv[1]);
        while (client.running())
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } catch (const std::exception& error) {
        std::cerr << "[Client] fatal: " << error.what() << '\n';
        return 1;
    }
    return 0;
}