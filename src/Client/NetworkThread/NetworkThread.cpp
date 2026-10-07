#include "NetworkThread.hpp"
#include "Shared/Types.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>

CNetworkThread::CNetworkThread(asio::io_context& io, SSceneData& sceneData,
                               SPLayerActions& playerActions, std::mutex& sceneMutex,
                               std::atomic_bool& running, std::condition_variable& stopCondition,
                               const std::string& serverAddress) :
    mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition), mIo(io), mSocket(io), mTcpSocket(io),
    mServerEndpoint(asio::ip::make_address_v4(serverAddress), Net::DefaultUdpPort) {}

CNetworkThread::~CNetworkThread() {
    join();
}

void CNetworkThread::start() {
    if (!mThread.joinable()) {
        mThread = std::thread(&CNetworkThread::run, this);
    }
}

void CNetworkThread::join() {
    if (mThread.joinable()) {
        mThread.join();
    }
}

void CNetworkThread::run() {
    try {
        constexpr auto                 networkInterval = std::chrono::milliseconds(16);
        std::array<std::uint8_t, 2048> buffer{};
        mSocket.open(asio::ip::udp::v4());
        mSocket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
        mSocket.non_blocking(true);

        mTcpSocket.connect(asio::ip::tcp::endpoint(mServerEndpoint.address(), Net::DefaultTcpPort));
        const auto udpPort = mSocket.local_endpoint().port();
        const auto hello   = Net::makeTcpHello(udpPort);
        asio::write(mTcpSocket, asio::buffer(hello));
        std::array<std::uint8_t, 5> welcome{};
        asio::read(mTcpSocket, asio::buffer(welcome));
        std::uint8_t assignedId = 0;
        if (!Net::readTcpWelcome(welcome.data(), welcome.size(), assignedId))
            throw std::runtime_error("invalid server handshake");
        mPlayerId = assignedId;
        {
            std::scoped_lock lock(mtex);
            mPlayerActions.connected.fill(false);
            mPlayerActions.connected.at(mPlayerId) = true;
        }
        mConnected.store(true);
        mStopCondition.notify_all();

        while (mRunning.load()) {
            SPlayerActionState action;
            {
                std::scoped_lock lock(mtex);
                action = mPlayerActions.states.at(mPlayerId);
            }
            const auto input = Net::makeInput(action);
            mSocket.send_to(asio::buffer(input), mServerEndpoint);

            asio::ip::udp::endpoint sender;
            for (;;) {
                asio::error_code error;
                const auto       size = mSocket.receive_from(asio::buffer(buffer), sender, 0, error);
                if (error == asio::error::would_block)
                    break;
                if (error)
                    break;
                if (!Net::validPacket(buffer.data(), size, Net::ePacketType::SNAPSHOT))
                    continue;
                std::scoped_lock                   lock(mtex);
                std::vector<Game::SSnapshotEntity> entities;
                std::size_t                        offset = Net::HeaderSize;
                while (offset + Net::CommandHeaderSize + sizeof(Net::Terminator) <= size &&
                       offset < size - sizeof(Net::Terminator)) {
                    if (buffer.at(offset) !=
                        static_cast<std::uint8_t>(Net::eCommandType::SNAPSHOT_ENTITY))
                        break;
                    Game::SSnapshotEntity entity;
                    std::uint64_t         entityId = 0;
                    std::memcpy(&entityId, buffer.data() + offset + 1, sizeof(entityId));
                    entity.id = static_cast<std::uint32_t>(entityId);
                    std::memcpy(&entity.kind, buffer.data() + offset + 9, sizeof(entity.kind));
                    std::memcpy(&entity.x, buffer.data() + offset + 10, sizeof(entity.x));
                    std::memcpy(&entity.y, buffer.data() + offset + 14, sizeof(entity.y));
                    std::memcpy(&entity.health, buffer.data() + offset + 18, sizeof(entity.health));
                    entities.push_back(entity);
                    offset += Net::CommandHeaderSize + 11;
                }
                mSceneData.replaceEntities(std::move(entities));
                ++mSceneData.tick;
            }
            std::unique_lock lock(mtex);
            if (mStopCondition.wait_for(lock, networkInterval, [this] { return !mRunning.load(); })) {
                break;
            }
        }
    } catch (const std::system_error& error) {
        mConnected.store(false);
        std::cerr << "[Client] Unable to connect to " << mServerEndpoint.address().to_string() << ':'
                  << Net::DefaultTcpPort << ": " << error.code().message() << '\n';
        mRunning.store(false);
        mStopCondition.notify_all();
    } catch (const std::exception& error) {
        mConnected.store(false);
        std::cerr << "[Client] Network error: " << error.what() << '\n';
        mRunning.store(false);
        mStopCondition.notify_all();
    }
}
