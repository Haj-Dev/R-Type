#include "NetworkThread.hpp"

#include "Shared/Types.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>

CServerNetworkThread::CServerNetworkThread(asio::io_context& io, SSceneData& sceneData,
                                           SPLayerActions& playerActions, std::mutex& sceneMutex,
                                           std::atomic_bool&        running,
                                           std::condition_variable& stopCondition) :
    mIo(io), mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition), mSocket(io), mAcceptor(io) {}

CServerNetworkThread::~CServerNetworkThread() {
    join();
}

void CServerNetworkThread::start() {
    if (!mThread.joinable()) {
        mThread = std::thread(&CServerNetworkThread::run, this);
    }
}

void CServerNetworkThread::join() {
    if (mThread.joinable()) {
        mThread.join();
    }
}

void CServerNetworkThread::run() {
    constexpr auto                 networkInterval = std::chrono::milliseconds(8);
    std::array<std::uint8_t, 2048> buffer{};
    asio::ip::udp::endpoint        sender;
    mSocket.open(asio::ip::udp::v4());
    mSocket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), Net::DefaultUdpPort));
    mSocket.non_blocking(true);
    mAcceptor.open(asio::ip::tcp::v4());
    mAcceptor.set_option(asio::socket_base::reuse_address(true));
    mAcceptor.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), Net::DefaultTcpPort));
    mAcceptor.listen();
    mAcceptor.non_blocking(true);
    std::cout << "[Server] Listening for TCP handshakes on port " << Net::DefaultTcpPort
              << " and UDP gameplay on port " << Net::DefaultUdpPort << '\n';

    while (mRunning.load()) {
        for (;;) {
            asio::error_code      error;
            asio::ip::tcp::socket socket(mIo);
            const auto            acceptResult = mAcceptor.accept(socket, error);
            if (acceptResult)
                error = acceptResult;
            if (error == asio::error::would_block)
                break;
            if (error) {
                std::cerr << "[Server] Accept error: " << error.message() << '\n';
                break;
            }
            socket.non_blocking(true);
            mPending.push_back(SPendingHandshake{.socket = std::move(socket)});
        }

        for (auto pending = mPending.begin(); pending != mPending.end();) {
            asio::error_code error;
            const auto       received =
                pending->socket.receive(asio::buffer(pending->data.data() + pending->received,
                                                     pending->data.size() - pending->received),
                                        0, error);
            pending->received += received;
            if (error && error != asio::error::would_block) {
                pending = mPending.erase(pending);
                continue;
            }
            if (pending->received != pending->data.size()) {
                ++pending;
                continue;
            }

            std::uint16_t udpPort  = 0;
            const auto    freeSlot = std::ranges::find_if(
                mClients, [](const auto& client) { return client.address().is_unspecified(); });
            if (!Net::readTcpHello(pending->data.data(), pending->received, udpPort) ||
                (freeSlot == mClients.end() && mClients.size() >= SPLayerActions::MaxPlayers)) {
                std::cerr << "[Server] Rejected invalid or excess TCP handshake\n";
                pending = mPending.erase(pending);
                continue;
            }
            const auto address  = pending->socket.remote_endpoint().address();
            const auto playerId = freeSlot == mClients.end() ?
                static_cast<std::uint8_t>(mClients.size()) :
                static_cast<std::uint8_t>(std::distance(mClients.begin(), freeSlot));
            if (freeSlot == mClients.end()) {
                mClients.emplace_back(address, udpPort);
                mControlSockets.push_back(std::move(pending->socket));
            } else {
                *freeSlot                    = asio::ip::udp::endpoint(address, udpPort);
                mControlSockets.at(playerId) = std::move(pending->socket);
            }
            {
                std::scoped_lock lock(mtex);
                mPlayerActions.connected.at(playerId) = true;
            }
            const auto welcome = Net::makeTcpWelcome(playerId);
            asio::write(mControlSockets.at(playerId), asio::buffer(welcome));
            std::cout << "[Server] Client " << address << ':' << udpPort << " connected as player "
                      << static_cast<unsigned>(playerId) << '\n';
            pending = mPending.erase(pending);
        }

        for (std::size_t i = mControlSockets.size(); i-- > 0;) {
            if (mClients.at(i).address().is_unspecified())
                continue;

            std::array<std::uint8_t, 1> probe{};
            asio::error_code            error;
            mControlSockets.at(i).receive(asio::buffer(probe), asio::socket_base::message_peek, error);
            if (!error || error == asio::error::would_block)
                continue;

            const auto client = mClients.at(i);
            std::cerr << "[Server] Client " << client << " disconnected\n";
            {
                std::scoped_lock lock(mtex);
                mPlayerActions.connected.at(i) = false;
                mPlayerActions.states.at(i)    = SPlayerActionState{};
            }
            asio::error_code closeError;
            const auto       closeResult = mControlSockets.at(i).close(closeError);
            if (closeResult)
                closeError = closeResult;
            mClients.at(i) = asio::ip::udp::endpoint{};
        }

        for (;;) {
            asio::error_code error;
            const auto       size = mSocket.receive_from(asio::buffer(buffer), sender, 0, error);
            if (error == asio::error::would_block)
                break;
            if (error) {
                std::cerr << "[Server] Receive error: " << error.message() << '\n';
                break;
            }

            const auto type   = size >= Net::HeaderSize ? static_cast<Net::ePacketType>(buffer.at(3)) :
                                                          Net::ePacketType::SNAPSHOT;
            auto       client = std::ranges::find(mClients, sender);
            if (type == Net::ePacketType::INPUT && client != mClients.end()) {
                SPlayerActionState action;
                if (Net::readInput(buffer.data(), size, action)) {
                    const auto id = static_cast<std::size_t>(std::distance(mClients.begin(), client));
                    std::scoped_lock lock(mtex);
                    mPlayerActions.states.at(id) = action;
                } else {
                    std::cerr << "[Server] Discarded malformed input from " << sender << '\n';
                }
            } else if (type == Net::ePacketType::INPUT) {
                std::cerr << "[Server] Ignored input from unknown client " << sender << '\n';
            }
        }

        std::vector<std::uint8_t> snapshot;
        {
            std::scoped_lock lock(mtex);
            snapshot = Net::makeSnapshot(mSceneData);
        }
        for (const auto& client : mClients) {
            if (!client.address().is_unspecified())
                mSocket.send_to(asio::buffer(snapshot), client);
        }

        std::unique_lock lock(mtex);
        if (mStopCondition.wait_for(lock, networkInterval, [this] { return !mRunning.load(); })) {
            break;
        }
    }
}
