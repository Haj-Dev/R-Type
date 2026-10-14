#include "NetworkThread.hpp"
#include "Shared/Types.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>

CNetworkThread::CNetworkThread(asio::io_context& io, SSceneData& sceneData,
                               SPLayerActions& playerActions, std::mutex& sceneMutex,
                               std::atomic_bool& running, std::condition_variable& stopCondition,
                               const std::string& serverAddress) :
    mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition), mSocket(io), mTcpSocket(io),
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
                    entity.id                              = static_cast<std::uint32_t>(entityId);
                    const auto            payload          = offset + Net::CommandHeaderSize;
                    constexpr std::size_t fixedPayloadSize = sizeof(entity.kind) + sizeof(entity.x) +
                        sizeof(entity.y) + sizeof(entity.health) + sizeof(entity.layer) +
                        sizeof(entity.scale) + sizeof(entity.offsetX) + sizeof(entity.offsetY) +
                        sizeof(entity.rotation) + sizeof(entity.atlas) +
                        sizeof(entity.baseDimension.width) + sizeof(entity.baseDimension.height) +
                        sizeof(std::uint64_t) + sizeof(entity.frameDuration) + sizeof(std::uint64_t);
                    if (payload + fixedPayloadSize >= size - sizeof(Net::Terminator))
                        break;
                    std::memcpy(&entity.kind, buffer.data() + payload, sizeof(entity.kind));
                    std::memcpy(&entity.x, buffer.data() + payload + 1, sizeof(entity.x));
                    std::memcpy(&entity.y, buffer.data() + payload + 5, sizeof(entity.y));
                    std::memcpy(&entity.health, buffer.data() + payload + 9, sizeof(entity.health));
                    std::size_t cursor = payload + 11;
                    std::memcpy(&entity.layer, buffer.data() + cursor, sizeof(entity.layer));
                    cursor += sizeof(entity.layer);
                    std::memcpy(&entity.scale, buffer.data() + cursor, sizeof(entity.scale));
                    cursor += sizeof(entity.scale);
                    std::memcpy(&entity.offsetX, buffer.data() + cursor, sizeof(entity.offsetX));
                    cursor += sizeof(entity.offsetX);
                    std::memcpy(&entity.offsetY, buffer.data() + cursor, sizeof(entity.offsetY));
                    cursor += sizeof(entity.offsetY);
                    std::memcpy(&entity.rotation, buffer.data() + cursor, sizeof(entity.rotation));
                    cursor += sizeof(entity.rotation);
                    std::memcpy(&entity.atlas, buffer.data() + cursor, sizeof(entity.atlas));
                    cursor += sizeof(entity.atlas);
                    std::memcpy(&entity.baseDimension.width, buffer.data() + cursor,
                                sizeof(entity.baseDimension.width));
                    cursor += sizeof(entity.baseDimension.width);
                    std::memcpy(&entity.baseDimension.height, buffer.data() + cursor,
                                sizeof(entity.baseDimension.height));
                    cursor += sizeof(entity.baseDimension.height);
                    std::uint64_t wireFrames = 0;
                    std::memcpy(&wireFrames, buffer.data() + cursor, sizeof(wireFrames));
                    entity.frames = static_cast<std::size_t>(wireFrames);
                    cursor += sizeof(wireFrames);
                    std::memcpy(&entity.frameDuration, buffer.data() + cursor,
                                sizeof(entity.frameDuration));
                    cursor += sizeof(entity.frameDuration);
                    std::uint64_t wireVariant = 0;
                    std::memcpy(&wireVariant, buffer.data() + cursor, sizeof(wireVariant));
                    entity.variant = static_cast<std::size_t>(wireVariant);
                    cursor += sizeof(wireVariant);
                    const auto* end = std::find(buffer.data() + cursor,
                                                buffer.data() + size - sizeof(Net::Terminator), '\n');
                    if (end == buffer.data() + size - sizeof(Net::Terminator))
                        break;
                    entity.texture.assign(reinterpret_cast<const char*>(buffer.data() + cursor),
                                          static_cast<std::size_t>(end - (buffer.data() + cursor)));
                    entities.push_back(entity);
                    offset = static_cast<std::size_t>(end - buffer.data()) + 1;
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
