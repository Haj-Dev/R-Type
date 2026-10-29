#include "NetworkThread.hpp"

#include "Shared/FixedRate.hpp"
#include "Shared/Types.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

CServerNetworkThread::CServerNetworkThread(
    asio::io_context& io, SSceneData& sceneData, SPLayerActions& playerActions, std::mutex& sceneMutex,
    std::atomic_bool& running, std::condition_variable& stopCondition, std::atomic_bool& gameActive,
    std::uint16_t tcpPort, std::uint16_t udpPort, SServerStatus& serverStatus, std::mutex& statusMutex) :
    mIo(io), mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition), mGameActive(gameActive), mTcpPort(tcpPort), mUdpPort(udpPort),
    mServerStatus(serverStatus), mStatusMutex(statusMutex), mSocket(io), mAcceptor(io) {}

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
    constexpr auto                       networkInterval = std::chrono::nanoseconds(1'000'000'000 / 120);

    const Timing::CScopedTimerResolution timerResolution;
    Timing::CFixedRate                   networkRate(networkInterval);
    std::vector<std::uint8_t>            buffer(Net::MaxUdpPayloadSize);
    asio::ip::udp::endpoint              sender;
    mSocket.open(asio::ip::udp::v4());
    mSocket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), mUdpPort));
    mSocket.non_blocking(true);
    mAcceptor.open(asio::ip::tcp::v4());
    mAcceptor.set_option(asio::socket_base::reuse_address(true));
    mAcceptor.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), mTcpPort));
    mAcceptor.listen();
    mAcceptor.non_blocking(true);
    std::cout << "[Server] Listening for TCP handshakes on port " << mTcpPort
              << " and UDP gameplay on port " << mUdpPort << '\n';
    updateServerStatus();

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
                (freeSlot == mClients.end() && mClients.size() >= Room::MaxConnections)) {
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
            mTcpBuffers.at(playerId).clear();
            mConnectionGameplayIds.at(playerId).reset();
            sendFrame(playerId, Net::makeTcpWelcome(playerId));
            std::cout << "[Server] Client " << address << ':' << udpPort << " connected as player "
                      << static_cast<unsigned>(playerId) << '\n';
            pending = mPending.erase(pending);
        }

        for (std::size_t index = 0; index < mControlSockets.size(); ++index) {
            if (!mControlSockets.at(index).is_open())
                continue;
            processLobby(static_cast<std::uint8_t>(index));
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
                const auto connectionId =
                    static_cast<std::uint8_t>(std::distance(mClients.begin(), client));
                const auto gameplayId = mConnectionGameplayIds.at(connectionId);
                if (!gameplayId)
                    continue;
                const auto activeRoom = mRoomManager.activeRoom();
                if (!activeRoom || mRoomManager.roomIdOfPlayer(connectionId) != activeRoom)
                    continue;
                SPlayerActionState action;
                if (Net::readInput(buffer.data(), size, action)) {
                    std::scoped_lock lock(mtex);
                    mPlayerActions.states.at(*gameplayId) = action;
                } else {
                    std::cerr << "[Server] Discarded malformed input from " << sender << '\n';
                }
            } else if (type == Net::ePacketType::INPUT) {
                std::cerr << "[Server] Ignored input from unknown client " << sender << '\n';
            }
        }

        const auto activeRoom = mRoomManager.activeRoom();
        if (activeRoom) {
            std::vector<std::uint8_t> snapshot;
            {
                std::scoped_lock lock(mtex);
                snapshot = Net::makeSnapshot(mSceneData);
            }
            for (const auto playerId : mRoomManager.playersInRoom(*activeRoom)) {
                if (playerId < mClients.size() && !mClients.at(playerId).address().is_unspecified())
                    mSocket.send_to(asio::buffer(snapshot), mClients.at(playerId));
            }
        }

        std::unique_lock lock(mtex);
        if (!networkRate.wait(mStopCondition, lock, [this] { return !mRunning.load(); }))
            break;
    }
}

void CServerNetworkThread::processLobby(std::uint8_t playerId) {
    auto& socket = mControlSockets.at(playerId);
    if (!socket.is_open())
        return;

    std::array<std::uint8_t, 1024> buffer{};
    for (;;) {
        asio::error_code error;
        const auto       received = socket.receive(asio::buffer(buffer), 0, error);
        if (error == asio::error::would_block || error == asio::error::try_again)
            break;
        if (error) {
            disconnectPlayer(playerId);
            return;
        }
        if (received == 0)
            break;
        auto& tcpBuffer = mTcpBuffers.at(playerId);
        tcpBuffer.insert(tcpBuffer.end(), buffer.begin(), buffer.begin() + received);
    }

    auto&       tcpBuffer = mTcpBuffers.at(playerId);
    std::size_t offset    = 0;
    while (tcpBuffer.size() - offset >= Net::LobbyHeaderSize) {
        Net::eLobbyCommand command;
        std::uint16_t      payloadLength = 0;
        if (!Net::readLobbyHeader(tcpBuffer.data() + offset, tcpBuffer.size() - offset, command,
                                  payloadLength)) {
            disconnectPlayer(playerId);
            return;
        }
        const auto frameSize = Net::LobbyHeaderSize + payloadLength;
        if (tcpBuffer.size() - offset < frameSize)
            break;
        handleLobbyCommand(playerId, command, tcpBuffer.data() + offset + Net::LobbyHeaderSize,
                           payloadLength);
        if (!mControlSockets.at(playerId).is_open())
            return;
        offset += frameSize;
    }
    if (offset > 0)
        tcpBuffer.erase(tcpBuffer.begin(), tcpBuffer.begin() + static_cast<std::ptrdiff_t>(offset));
}

void CServerNetworkThread::handleLobbyCommand(std::uint8_t playerId, Net::eLobbyCommand command,
                                              const std::uint8_t* payload, std::size_t payloadSize) {
    switch (command) {
        case Net::eLobbyCommand::JOIN: {
            std::string name;
            if (!Net::readJoin(payload, payloadSize, name)) {
                sendRejection(playerId, Room::eRejectReason::INVALID_NAME, "Invalid player name");
                return;
            }
            const auto result = mRoomManager.setPlayerName(playerId, name);
            if (!result.ok) {
                sendRejection(playerId, result.reason, result.message);
                return;
            }
            updateServerStatus();
            sendRoomList(playerId);
            return;
        }
        case Net::eLobbyCommand::ROOM_LIST_REQUEST: {
            if (!Net::readRoomListRequest(payload, payloadSize)) {
                sendRejection(playerId, Room::eRejectReason::SERVER_ERROR, "Invalid room list request");
                return;
            }
            sendRoomList(playerId);
            return;
        }
        case Net::eLobbyCommand::CREATE_ROOM: {
            std::string roomName;
            if (!Net::readCreateRoom(payload, payloadSize, roomName)) {
                sendRejection(playerId, Room::eRejectReason::INVALID_ROOM_NAME, "Invalid room name");
                return;
            }
            const auto result = mRoomManager.createRoom(playerId, roomName);
            if (!result.ok) {
                sendRejection(playerId, result.reason, result.message);
                return;
            }
            updateServerStatus();
            const auto room = mRoomManager.roomOfPlayer(playerId);
            if (room)
                sendRoomInfo(playerId, Net::eLobbyCommand::ROOM_JOINED, *room);
            return;
        }
        case Net::eLobbyCommand::SELECT_ROOM: {
            std::uint16_t roomId = 0;
            if (!Net::readSelectRoom(payload, payloadSize, roomId)) {
                sendRejection(playerId, Room::eRejectReason::ROOM_NOT_FOUND, "Invalid room selection");
                return;
            }
            const auto result = mRoomManager.joinRoom(playerId, roomId);
            if (!result.ok) {
                sendRejection(playerId, result.reason, result.message);
                return;
            }
            updateServerStatus();
            const auto room = mRoomManager.roomOfPlayer(playerId);
            if (room) {
                sendRoomInfo(playerId, Net::eLobbyCommand::ROOM_JOINED, *room);
                broadcastRoom(roomId, Net::makeRoomInfo(Net::eLobbyCommand::ROOM_STATE, *room),
                              playerId);
            }
            return;
        }
        case Net::eLobbyCommand::LEAVE_ROOM: {
            if (!Net::readLeaveRoom(payload, payloadSize)) {
                sendRejection(playerId, Room::eRejectReason::SERVER_ERROR, "Invalid leave request");
                return;
            }
            const auto roomId = mRoomManager.roomIdOfPlayer(playerId);
            const auto result = mRoomManager.leaveRoom(playerId);
            if (!result.ok) {
                sendRejection(playerId, result.reason, result.message);
                return;
            }
            updateServerStatus();
            {
                const auto gameplayId = mConnectionGameplayIds.at(playerId);
                if (gameplayId) {
                    std::scoped_lock lock(mtex);
                    mPlayerActions.connected.at(*gameplayId) = false;
                    mPlayerActions.states.at(*gameplayId)    = SPlayerActionState{};
                }
                mConnectionGameplayIds.at(playerId).reset();
            }
            sendFrame(playerId, Net::makeRoomLeft());
            sendRoomList(playerId);
            if (roomId) {
                const auto room = mRoomManager.roomInfo(*roomId);
                if (room)
                    broadcastRoom(*roomId, Net::makeRoomInfo(Net::eLobbyCommand::ROOM_STATE, *room));
            }
            if (!mRoomManager.activeRoom())
                mGameActive.store(false);
            return;
        }
        case Net::eLobbyCommand::LAUNCH_GAME: {
            if (!Net::readLaunchGame(payload, payloadSize)) {
                sendRejection(playerId, Room::eRejectReason::SERVER_ERROR, "Invalid launch request");
                return;
            }
            const auto roomId = mRoomManager.roomIdOfPlayer(playerId);
            const auto result = mRoomManager.launch(playerId);
            if (!result.ok) {
                sendRejection(playerId, result.reason, result.message);
                return;
            }
            updateServerStatus();
            if (!roomId)
                return;
            const auto room = mRoomManager.roomInfo(*roomId);
            if (!room)
                return;
            std::uint8_t gameplayId = 0;
            for (const auto& player : room->players) {
                mConnectionGameplayIds.at(player.id) = gameplayId;
                sendFrame(player.id, Net::makeGameStarted(*roomId, gameplayId));
                {
                    std::scoped_lock lock(mtex);
                    mPlayerActions.connected.at(gameplayId) = true;
                    mPlayerActions.states.at(gameplayId)    = SPlayerActionState{};
                }
                ++gameplayId;
            }
            mGameActive.store(true);
            std::cout << "[Server] Room " << *roomId << " launched with " << room->players.size()
                      << " player(s)\n";
            return;
        }
        default:
            std::cerr << "[Server] Unknown lobby command " << static_cast<unsigned>(command)
                      << " from player " << static_cast<unsigned>(playerId) << '\n';
            disconnectPlayer(playerId);
            return;
    }
}

void CServerNetworkThread::sendFrame(std::uint8_t playerId, const std::vector<std::uint8_t>& frame) {
    if (frame.empty() || playerId >= mControlSockets.size())
        return;
    auto& socket = mControlSockets.at(playerId);
    if (!socket.is_open())
        return;

    asio::error_code blockingError;
    if (socket.non_blocking(false, blockingError)) {
        std::cerr << "[Server] Failed to set socket to blocking mode for player "
                  << static_cast<unsigned>(playerId) << ": " << blockingError.message() << '\n';
        return;
    }
    asio::error_code writeError;
    asio::write(socket, asio::buffer(frame), writeError);
    asio::error_code nonBlockingError;
    if (socket.non_blocking(true, nonBlockingError)) {
        std::cerr << "[Server] Failed to set socket back to non-blocking mode for player "
                  << static_cast<unsigned>(playerId) << ": " << nonBlockingError.message() << '\n';
    }
    if (writeError) {
        std::cerr << "[Server] Failed to send lobby frame to player " << static_cast<unsigned>(playerId)
                  << ": " << writeError.message() << '\n';
    }
}

void CServerNetworkThread::sendRoomList(std::uint8_t playerId) {
    sendFrame(playerId, Net::makeRoomList(mRoomManager.listRooms()));
}

void CServerNetworkThread::sendRoomInfo(std::uint8_t playerId, Net::eLobbyCommand command,
                                        const Room::SInfo& room) {
    sendFrame(playerId, Net::makeRoomInfo(command, room));
}

void CServerNetworkThread::sendRejection(std::uint8_t playerId, Room::eRejectReason reason,
                                         const std::string& message) {
    sendFrame(playerId, Net::makeRoomRejected(reason, message));
}

void CServerNetworkThread::broadcastRoom(std::uint16_t roomId, const std::vector<std::uint8_t>& frame,
                                         std::optional<std::uint8_t> except) {
    for (const auto playerId : mRoomManager.playersInRoom(roomId)) {
        if (except && *except == playerId)
            continue;
        sendFrame(playerId, frame);
    }
}

void CServerNetworkThread::disconnectPlayer(std::uint8_t playerId) {
    if (playerId >= mControlSockets.size())
        return;
    const auto roomId = mRoomManager.roomIdOfPlayer(playerId);
    mRoomManager.removePlayer(playerId);
    updateServerStatus();
    {
        const auto gameplayId = mConnectionGameplayIds.at(playerId);
        if (gameplayId) {
            std::scoped_lock lock(mtex);
            mPlayerActions.connected.at(*gameplayId) = false;
            mPlayerActions.states.at(*gameplayId)    = SPlayerActionState{};
        }
        mConnectionGameplayIds.at(playerId).reset();
    }
    asio::error_code error;
    if (mControlSockets.at(playerId).close(error)) {
        std::cerr << "[Server] Failed to close player socket: " << error.message() << '\n';
    }
    if (playerId < mClients.size())
        mClients.at(playerId) = asio::ip::udp::endpoint{};
    mTcpBuffers.at(playerId).clear();
    if (roomId) {
        const auto room = mRoomManager.roomInfo(*roomId);
        if (room)
            broadcastRoom(*roomId, Net::makeRoomInfo(Net::eLobbyCommand::ROOM_STATE, *room));
    }
    if (!mRoomManager.activeRoom())
        mGameActive.store(false);
    std::cerr << "[Server] Player " << static_cast<unsigned>(playerId) << " disconnected\n";
}

void CServerNetworkThread::updateServerStatus() {
    std::scoped_lock lock(mStatusMutex);
    mServerStatus.rooms            = mRoomManager.roomCount();
    mServerStatus.connectedPlayers = mRoomManager.connectedPlayerCount();
    mServerStatus.maxPlayers       = Room::MaxConnections;
    mServerStatus.joinableRooms    = mRoomManager.joinableRoomCount();
    mServerStatus.state            = Net::Control::eServerState::READY;
}
