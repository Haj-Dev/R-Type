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
                               const std::string& serverAddress, Lobby::SLobbyState& lobbyState,
                               std::deque<Lobby::SCommand>& lobbyCommands) :
    mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition), mLobbyState(lobbyState), mLobbyCommands(lobbyCommands), mSocket(io),
    mTcpSocket(io), mServerEndpoint(asio::ip::make_address_v4(serverAddress), Net::DefaultUdpPort) {}

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
        mTcpSocket.non_blocking(true);
        {
            std::scoped_lock lock(mtex);
            mLobbyState.phase    = Lobby::eLobbyPhase::NAME_ENTRY;
            mLobbyState.playerId = mPlayerId;
            mLobbyState.message.clear();
        }
        mConnected.store(true);
        mStopCondition.notify_all();

        while (mRunning.load()) {
            processLobbyCommands();
            processLobbyMessages();
            if (mInGame) {
                sendInput();
                receiveSnapshots(buffer);
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
        {
            std::scoped_lock lock(mtex);
            mLobbyState.phase   = Lobby::eLobbyPhase::ERROR;
            mLobbyState.message = error.code().message();
        }
        mRunning.store(false);
        mStopCondition.notify_all();
    } catch (const std::exception& error) {
        mConnected.store(false);
        std::cerr << "[Client] Network error: " << error.what() << '\n';
        {
            std::scoped_lock lock(mtex);
            mLobbyState.phase   = Lobby::eLobbyPhase::ERROR;
            mLobbyState.message = error.what();
        }
        mRunning.store(false);
        mStopCondition.notify_all();
    }
}

void CNetworkThread::processLobbyCommands() {
    std::deque<Lobby::SCommand> commands;
    {
        std::scoped_lock lock(mtex);
        commands.swap(mLobbyCommands);
    }
    for (const auto& command : commands) {
        if (!mRunning.load())
            return;
        switch (command.type) {
            case Lobby::eClientCommand::JOIN:
                sendLobbyFrame(Net::makeJoin(command.text));
                {
                    std::scoped_lock lock(mtex);
                    mLobbyState.playerName = command.text;
                    mLobbyState.message.clear();
                }
                break;
            case Lobby::eClientCommand::CREATE_ROOM:
                sendLobbyFrame(Net::makeCreateRoom(command.text));
                break;
            case Lobby::eClientCommand::SELECT_ROOM:
                sendLobbyFrame(Net::makeSelectRoom(command.roomId));
                break;
            case Lobby::eClientCommand::LAUNCH_GAME: sendLobbyFrame(Net::makeLaunchGame()); break;
            case Lobby::eClientCommand::LEAVE_ROOM: sendLobbyFrame(Net::makeLeaveRoom()); break;
            case Lobby::eClientCommand::REFRESH_ROOMS: sendLobbyFrame(Net::makeRoomListRequest()); break;
        }
    }
}

void CNetworkThread::processLobbyMessages() {
    std::array<std::uint8_t, 1024> buffer{};
    for (;;) {
        asio::error_code error;
        const auto       received = mTcpSocket.receive(asio::buffer(buffer), 0, error);
        if (error == asio::error::would_block || error == asio::error::try_again)
            break;
        if (error)
            throw std::system_error(error);
        if (received == 0)
            break;
        mTcpBuffer.insert(mTcpBuffer.end(), buffer.begin(), buffer.begin() + received);
    }

    std::size_t offset = 0;
    while (mTcpBuffer.size() - offset >= Net::LobbyHeaderSize) {
        Net::eLobbyCommand command;
        std::uint16_t      payloadLength = 0;
        if (!Net::readLobbyHeader(mTcpBuffer.data() + offset, mTcpBuffer.size() - offset, command,
                                  payloadLength))
            throw std::runtime_error("invalid lobby frame");
        const auto frameSize = Net::LobbyHeaderSize + payloadLength;
        if (mTcpBuffer.size() - offset < frameSize)
            break;
        handleLobbyMessage(command, mTcpBuffer.data() + offset + Net::LobbyHeaderSize, payloadLength);
        offset += frameSize;
    }
    if (offset > 0)
        mTcpBuffer.erase(mTcpBuffer.begin(), mTcpBuffer.begin() + static_cast<std::ptrdiff_t>(offset));
}

void CNetworkThread::handleLobbyMessage(Net::eLobbyCommand command, const std::uint8_t* payload,
                                        std::size_t payloadSize) {
    switch (command) {
        case Net::eLobbyCommand::ROOM_LIST: {
            std::vector<Room::SSummary> rooms;
            if (!Net::readRoomList(payload, payloadSize, rooms))
                throw std::runtime_error("invalid room list");
            std::scoped_lock lock(mtex);
            mLobbyState.rooms = std::move(rooms);
            if (mLobbyState.phase != Lobby::eLobbyPhase::IN_ROOM)
                mLobbyState.phase = Lobby::eLobbyPhase::ROOM_LIST;
            mLobbyState.message.clear();
            break;
        }
        case Net::eLobbyCommand::ROOM_JOINED: {
            Room::SInfo room;
            if (!Net::readRoomInfo(payload, payloadSize, room))
                throw std::runtime_error("invalid room info");
            std::scoped_lock lock(mtex);
            mLobbyState.currentRoom = std::move(room);
            mLobbyState.phase       = Lobby::eLobbyPhase::IN_ROOM;
            mLobbyState.message.clear();
            break;
        }
        case Net::eLobbyCommand::ROOM_STATE: {
            Room::SInfo room;
            if (!Net::readRoomInfo(payload, payloadSize, room))
                throw std::runtime_error("invalid room state");
            std::scoped_lock lock(mtex);
            if (mLobbyState.currentRoom && mLobbyState.currentRoom->id == room.id)
                mLobbyState.currentRoom = std::move(room);
            break;
        }
        case Net::eLobbyCommand::ROOM_REJECTED: {
            Room::eRejectReason reason;
            std::string         message;
            if (!Net::readRoomRejected(payload, payloadSize, reason, message))
                throw std::runtime_error("invalid room rejection");
            std::scoped_lock lock(mtex);
            mLobbyState.message = std::move(message);
            break;
        }
        case Net::eLobbyCommand::ROOM_LEFT: {
            if (!Net::readRoomLeft(payload, payloadSize))
                throw std::runtime_error("invalid room left");
            mInGame = false;
            std::scoped_lock lock(mtex);
            mLobbyState.currentRoom.reset();
            mLobbyState.phase = Lobby::eLobbyPhase::ROOM_LIST;
            mPlayerActions.connected.fill(false);
            break;
        }
        case Net::eLobbyCommand::GAME_STARTED: {
            std::uint16_t roomId = 0;
            if (!Net::readGameStarted(payload, payloadSize, roomId))
                throw std::runtime_error("invalid game start");
            {
                std::scoped_lock lock(mtex);
                mLobbyState.phase = Lobby::eLobbyPhase::IN_GAME;
                mPlayerActions.connected.fill(false);
                mPlayerActions.connected.at(mPlayerId) = true;
                mPlayerActions.states.at(mPlayerId)    = SPlayerActionState{};
            }
            mInGame = true;
            mStopCondition.notify_all();
            break;
        }
        default: throw std::runtime_error("unexpected lobby command");
    }
}

void CNetworkThread::sendLobbyFrame(const std::vector<std::uint8_t>& frame) {
    if (frame.empty())
        return;
    asio::error_code blockingError;
    if (mTcpSocket.non_blocking(false, blockingError)) {
        std::cerr << "[Client] Failed to set blocking mode: " << blockingError.message() << '\n';
    }
    asio::error_code writeError;
    asio::write(mTcpSocket, asio::buffer(frame), writeError);
    asio::error_code nonBlockingError;
    if (mTcpSocket.non_blocking(true, nonBlockingError)) {
        std::cerr << "[Client] Failed to restore non-blocking mode: " << nonBlockingError.message()
                  << '\n';
    }
    if (writeError) {
        std::cerr << "[Client] Failed to send lobby frame: " << writeError.message() << '\n';
        mRunning.store(false);
        mStopCondition.notify_all();
    }
}

void CNetworkThread::sendInput() {
    SPlayerActionState action;
    {
        std::scoped_lock lock(mtex);
        action = mPlayerActions.states.at(mPlayerId);
    }
    const auto input = Net::makeInput(action);
    mSocket.send_to(asio::buffer(input), mServerEndpoint);
}

void CNetworkThread::receiveSnapshots(std::array<std::uint8_t, 2048>& buffer) {
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
            if (buffer.at(offset) != static_cast<std::uint8_t>(Net::eCommandType::SNAPSHOT_ENTITY))
                break;
            Game::SSnapshotEntity entity;
            std::uint64_t         entityId = 0;
            std::memcpy(&entityId, buffer.data() + offset + 1, sizeof(entityId));
            entity.id                              = static_cast<std::uint32_t>(entityId);
            const auto            payload          = offset + Net::CommandHeaderSize;
            constexpr std::size_t fixedPayloadSize = sizeof(entity.kind) + sizeof(entity.x) +
                sizeof(entity.y) + sizeof(entity.health) + sizeof(entity.layer) + sizeof(entity.scale) +
                sizeof(entity.offsetX) + sizeof(entity.offsetY) + sizeof(entity.rotation) +
                sizeof(entity.atlas) + sizeof(entity.baseDimension.width) +
                sizeof(entity.baseDimension.height) + sizeof(std::uint64_t) +
                sizeof(entity.frameDuration) + sizeof(std::uint64_t);
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
            std::memcpy(&entity.frameDuration, buffer.data() + cursor, sizeof(entity.frameDuration));
            cursor += sizeof(entity.frameDuration);
            std::uint64_t wireVariant = 0;
            std::memcpy(&wireVariant, buffer.data() + cursor, sizeof(wireVariant));
            entity.variant = static_cast<std::size_t>(wireVariant);
            cursor += sizeof(wireVariant);
            const auto* end =
                std::find(buffer.data() + cursor, buffer.data() + size - sizeof(Net::Terminator), '\n');
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
}
