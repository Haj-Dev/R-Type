#include "NetworkThread.hpp"
#include "Shared/ControlProtocol.hpp"
#include "Shared/FixedRate.hpp"
#include "Shared/Types.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <vector>

CNetworkThread::CNetworkThread(asio::io_context& io, SSceneData& sceneData,
                               SPLayerActions& playerActions, std::mutex& sceneMutex,
                               std::atomic_bool& running, std::condition_variable& stopCondition,
                               const std::string& masterAddress, std::uint16_t masterPort,
                               const std::string& preferredServer, Lobby::SLobbyState& lobbyState,
                               std::deque<Lobby::SCommand>& lobbyCommands,
                               std::atomic_bool&            authoritativeState) :
    mIo(io), mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mAuthoritativeState(authoritativeState), mStopCondition(stopCondition),
    mMasterAddress(masterAddress), mMasterPort(masterPort), mPreferredServer(preferredServer),
    mLobbyState(lobbyState), mLobbyCommands(lobbyCommands), mSocket(io), mVoiceSocket(io),
    mTcpSocket(io), mAudioSystem(std::make_unique<Audio::AAudioSystem>(Audio::makeMiniaudioBackend())) {}

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
        mSocket.open(asio::ip::udp::v4());
        mSocket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
        mSocket.non_blocking(true);
        mVoiceSocket.open(asio::ip::udp::v4());
        mVoiceSocket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
        mVoiceSocket.non_blocking(true);
        if (!mAudioSystem->start())
            std::cerr << "[Client] Voice audio device unavailable\n";

        bool               selected = false;
        Lobby::SServerInfo server;
        while (mRunning.load() && !selected) {
            if (!discoverServers()) {
                std::unique_lock lock(mtex);
                if (mStopCondition.wait_for(lock, std::chrono::seconds(1),
                                            [this] { return !mRunning.load(); }))
                    return;
                continue;
            }
            const auto selection = waitForServerSelection();
            if (selection.result == eSelectionResult::STOPPED)
                return;
            if (selection.result == eSelectionResult::REFRESH)
                continue;
            server   = selection.server;
            selected = true;
        }
        if (!selected)
            return;

        if (!connectToGameServer(server))
            throw std::runtime_error("unable to connect to the selected server");

        mConnected.store(true);
        mStopCondition.notify_all();
        runGameSession();
    } catch (const std::system_error& error) {
        mConnected.store(false);
        std::cerr << "[Client] Network error: " << error.code().message() << '\n';
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

bool CNetworkThread::discoverServers() {
    {
        std::scoped_lock lock(mtex);
        mLobbyState.phase = Lobby::eLobbyPhase::CONNECTING;
        mLobbyState.message.clear();
    }

    try {
        asio::error_code      error;
        asio::ip::tcp::socket masterSocket(mIo);
        asio::ip::address     address = asio::ip::make_address(mMasterAddress, error);
        if (error) {
            asio::ip::tcp::resolver resolver(mIo);
            const auto results = resolver.resolve(mMasterAddress, std::to_string(mMasterPort), error);
            if (error || results.empty())
                return false;
            address = results.begin()->endpoint().address();
        }
        if (masterSocket.connect(asio::ip::tcp::endpoint(address, mMasterPort), error))
            return false;

        const auto request = Net::Control::makeServerListRequest(Net::Control::ProtocolMajor,
                                                                 Net::Control::ProtocolMinor);
        asio::write(masterSocket, asio::buffer(request), error);
        if (error)
            return false;

        std::array<std::uint8_t, Net::WireHeaderSize> header{};
        asio::read(masterSocket, asio::buffer(header), error);
        if (error)
            return false;
        Net::Control::eCommand command;
        std::uint16_t          payloadLength = 0;
        if (!Net::Control::readHeader(header.data(), header.size(), command, payloadLength))
            return false;
        std::vector<std::uint8_t> payload(payloadLength);
        if (payloadLength > 0) {
            asio::read(masterSocket, asio::buffer(payload), error);
            if (error)
                return false;
        }

        if (command == Net::Control::eCommand::ERROR) {
            Net::Control::SError masterError;
            if (Net::Control::readError(payload.data(), payload.size(), masterError)) {
                std::scoped_lock lock(mtex);
                mLobbyState.message = masterError.message;
            }
            return false;
        }
        if (command != Net::Control::eCommand::SERVER_LIST)
            return false;

        Net::Control::SServerList list;
        if (!Net::Control::readServerList(payload.data(), payload.size(), list))
            return false;

        std::vector<Lobby::SServerInfo> servers;
        servers.reserve(list.entries.size());
        for (const auto& entry : list.entries) {
            servers.push_back(Lobby::SServerInfo{
                .name             = entry.name,
                .address          = entry.address,
                .tcpPort          = entry.tcpPort,
                .udpPort          = entry.udpPort,
                .connectedPlayers = entry.connectedPlayers,
                .maxPlayers       = entry.maxPlayers,
                .rooms            = entry.rooms,
                .joinableRooms    = entry.joinableRooms,
                .major            = entry.major,
                .minor            = entry.minor,
                .state            = static_cast<std::uint8_t>(entry.state),
            });
        }

        {
            std::scoped_lock lock(mtex);
            mLobbyState.servers = std::move(servers);
            mLobbyState.phase   = Lobby::eLobbyPhase::SERVER_SELECT;
            mLobbyState.message.clear();
        }
        return true;
    } catch (const std::exception& error) {
        std::scoped_lock lock(mtex);
        mLobbyState.message = error.what();
        return false;
    }
}

CNetworkThread::SSelection CNetworkThread::waitForServerSelection() {
    if (!mPreferredServer.empty()) {
        std::scoped_lock lock(mtex);
        for (const auto& server : mLobbyState.servers) {
            if (server.name == mPreferredServer)
                return SSelection{.result = eSelectionResult::SELECTED, .server = server};
        }
    }

    while (mRunning.load()) {
        std::deque<Lobby::SCommand> commands;
        {
            std::scoped_lock lock(mtex);
            commands.swap(mLobbyCommands);
        }
        for (const auto& command : commands) {
            if (command.type == Lobby::eClientCommand::SELECT_SERVER) {
                std::scoped_lock lock(mtex);
                for (const auto& server : mLobbyState.servers) {
                    if (server.name == command.text)
                        return SSelection{.result = eSelectionResult::SELECTED, .server = server};
                }
            }
            if (command.type == Lobby::eClientCommand::REFRESH_SERVERS)
                return SSelection{.result = eSelectionResult::REFRESH, .server = {}};
        }
        std::unique_lock lock(mtex);
        if (mStopCondition.wait_for(lock, std::chrono::milliseconds(16),
                                    [this] { return !mRunning.load(); }))
            break;
    }
    return SSelection{.result = eSelectionResult::STOPPED, .server = {}};
}

bool CNetworkThread::connectToGameServer(const Lobby::SServerInfo& server) {
    asio::error_code  error;
    asio::ip::address address = asio::ip::make_address(server.address, error);
    if (error) {
        asio::ip::tcp::resolver resolver(mIo);
        const auto results = resolver.resolve(server.address, std::to_string(server.tcpPort), error);
        if (error || results.empty())
            return false;
        address = results.begin()->endpoint().address();
    }

    mServerEndpoint      = asio::ip::udp::endpoint(address, server.udpPort);
    mVoiceServerEndpoint = asio::ip::udp::endpoint(address, server.udpPort + 1);
    mTcpBuffer.clear();
    if (mTcpSocket.close(error)) {
        std::cerr << "[Client] Failed to close previous server socket: " << error.message() << '\n';
    }
    mTcpSocket = asio::ip::tcp::socket(mIo);
    if (mTcpSocket.connect(asio::ip::tcp::endpoint(address, server.tcpPort), error))
        return false;

    const auto udpPort   = mSocket.local_endpoint().port();
    const auto voicePort = mVoiceSocket.local_endpoint().port();
    const auto hello     = Net::makeTcpHello(udpPort, voicePort);
    asio::write(mTcpSocket, asio::buffer(hello), error);
    if (error)
        return false;

    std::array<std::uint8_t, 5> welcome{};
    asio::read(mTcpSocket, asio::buffer(welcome), error);
    if (error)
        return false;
    std::uint8_t assignedId = 0;
    if (!Net::readTcpWelcome(welcome.data(), welcome.size(), assignedId))
        return false;
    mPlayerId = assignedId;
    if (mTcpSocket.non_blocking(true, error))
        return false;
    {
        std::scoped_lock lock(mtex);
        mLobbyState.phase    = Lobby::eLobbyPhase::NAME_ENTRY;
        mLobbyState.playerId = mPlayerId;
        mLobbyState.message.clear();
    }
    return true;
}

void CNetworkThread::runGameSession() {
    constexpr auto                       networkInterval = std::chrono::nanoseconds(1'000'000'000 / 60);
    const Timing::CScopedTimerResolution timerResolution;
    Timing::CFixedRate                   networkRate(networkInterval);
    std::vector<std::uint8_t>            buffer(Net::MaxUdpPayloadSize);
    while (mRunning.load()) {
        processLobbyCommands();
        processLobbyMessages();
        if (mInGame) {
            // A spectator has no character, so it sends nothing and only listens.
            if (!mSpectating)
                sendInput();
            receiveSnapshots(buffer);
            processVoice();
        }
        std::unique_lock lock(mtex);
        if (!networkRate.wait(mStopCondition, lock, [this] { return !mRunning.load(); }))
            break;
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
            case Lobby::eClientCommand::SPECTATE_ROOM:
                sendLobbyFrame(Net::makeSpectateRoom(command.roomId));
                break;
            case Lobby::eClientCommand::LAUNCH_GAME: sendLobbyFrame(Net::makeLaunchGame()); break;
            case Lobby::eClientCommand::LEAVE_ROOM: sendLobbyFrame(Net::makeLeaveRoom()); break;
            case Lobby::eClientCommand::REFRESH_ROOMS: sendLobbyFrame(Net::makeRoomListRequest()); break;
            case Lobby::eClientCommand::SELECT_SERVER:
            case Lobby::eClientCommand::REFRESH_SERVERS: break;
            case Lobby::eClientCommand::SEND_CHAT:
                sendLobbyFrame(Net::makeChatSend(SCChatMessage{0, 0, {}, command.text}));
                break;
            case Lobby::eClientCommand::TOGGLE_MIC:
                mLobbyState.microphoneEnabled = !mLobbyState.microphoneEnabled;
                break;
            case Lobby::eClientCommand::TOGGLE_DEAFEN:
                mLobbyState.deafened = !mLobbyState.deafened;
                for (std::uint8_t playerId = 0; playerId < Audio::MaxPlayers; ++playerId)
                    mAudioSystem->setPlayerMuted(playerId, mLobbyState.deafened);
                break;
            case Lobby::eClientCommand::VOLUME_UP:
                mLobbyState.voiceVolume = std::min(1.0F, mLobbyState.voiceVolume + 0.1F);
                for (std::uint8_t playerId = 0; playerId < Audio::MaxPlayers; ++playerId)
                    mAudioSystem->setPlayerVolume(playerId, mLobbyState.voiceVolume);
                break;
            case Lobby::eClientCommand::VOLUME_DOWN:
                mLobbyState.voiceVolume = std::max(0.0F, mLobbyState.voiceVolume - 0.1F);
                for (std::uint8_t playerId = 0; playerId < Audio::MaxPlayers; ++playerId)
                    mAudioSystem->setPlayerVolume(playerId, mLobbyState.voiceVolume);
                break;
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
            mChatSystem.clearMessages(mLobbyState.chat);
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
            mInGame     = false;
            mSpectating = false;
            mGameplayId = 0;
            mVoiceHasSequence.fill(false);
            std::scoped_lock lock(mtex);
            mLobbyState.currentRoom.reset();
            mChatSystem.clearMessages(mLobbyState.chat);
            mLobbyState.phase = Lobby::eLobbyPhase::ROOM_LIST;
            mPlayerActions.connected.fill(false);
            mLobbyState.gameplayId = 0;
            mAuthoritativeState.store(false);
            mSceneData.replaceEntities({});
            mSceneData.tick   = 0;
            mLastReceivedTick = 0;
            mHasReceivedTick  = false;
            break;
        }
        case Net::eLobbyCommand::CHAT_MESSAGE: {
            SCChatMessage message;
            if (!Net::readChatMessage(payload, payloadSize, message))
                throw std::runtime_error("invalid chat message");
            std::scoped_lock lock(mtex);
            mChatSystem.post(mLobbyState.chat, std::move(message));
            break;
        }
        case Net::eLobbyCommand::GAME_STARTED: {
            std::uint16_t roomId     = 0;
            std::uint8_t  gameplayId = 0;
            if (!Net::readGameStarted(payload, payloadSize, roomId, gameplayId))
                throw std::runtime_error("invalid game start");
            mSpectating = gameplayId == Room::SpectatorGameplayId;
            if (!mSpectating)
                mGameplayId = gameplayId;
            {
                std::scoped_lock lock(mtex);
                mLobbyState.phase      = Lobby::eLobbyPhase::IN_GAME;
                mLobbyState.gameplayId = gameplayId;
                mPlayerActions.connected.fill(false);
                if (!mSpectating) {
                    mPlayerActions.connected.at(mGameplayId) = true;
                    mPlayerActions.states.at(mGameplayId)    = SPlayerActionState{};
                    mSceneData.replaceEntities({});
                    mSceneData.tick = 0;
                }
            }
            mAuthoritativeState.store(false);
            mLastReceivedTick = 0;
            mHasReceivedTick  = false;
            mVoiceSequence    = 0;
            mVoiceHasSequence.fill(false);
            mInGame           = true;
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
        action = mPlayerActions.states.at(mGameplayId);
    }
    const auto input = Net::makeInput(action);
    mSocket.send_to(asio::buffer(input), mServerEndpoint);
}

void CNetworkThread::processVoice() {
    bool microphoneEnabled = false;
    bool deafened          = false;
    {
        std::scoped_lock lock(mtex);
        microphoneEnabled = mLobbyState.microphoneEnabled;
        deafened          = mLobbyState.deafened;
    }

    if (microphoneEnabled && mAudioSystem->running()) {
        std::array<float, Audio::SamplesPerFrame> samples{};
        if (mAudioSystem->captureVoice(samples)) {
            std::vector<std::uint8_t> encoded;
            if (mVoiceCodec.encode(samples, encoded)) {
                const auto       packet = Net::makeVoice(mGameplayId, mVoiceSequence++, encoded);
                asio::error_code error;
                mVoiceSocket.send_to(asio::buffer(packet), mVoiceServerEndpoint, 0, error);
                if (error)
                    std::cerr << "[Client] Failed to send voice frame: " << error.message() << '\n';
            }
        }
    }

    std::array<std::uint8_t, Net::MaxUdpPayloadSize> buffer{};
    for (;;) {
        asio::ip::udp::endpoint sender;
        asio::error_code        error;
        const auto              size = mVoiceSocket.receive_from(asio::buffer(buffer), sender, 0, error);
        if (error == asio::error::would_block || error == asio::error::try_again)
            break;
        if (error) {
            std::cerr << "[Client] Voice receive error: " << error.message() << '\n';
            break;
        }
        if (deafened)
            continue;
        std::uint8_t       playerId = 0;
        std::uint16_t      sequence = 0;
        std::vector<std::uint8_t> encoded;
        if (Net::readVoice(buffer.data(), size, playerId, sequence, encoded) &&
            playerId != mGameplayId) {
            const bool newerSequence =
                !mVoiceHasSequence.at(playerId) ||
                (sequence != mVoiceLastSequence.at(playerId) &&
                 static_cast<std::uint16_t>(sequence - mVoiceLastSequence.at(playerId)) < 0x8000U);
            if (!newerSequence)
                continue;
            mVoiceHasSequence.at(playerId) = true;
            mVoiceLastSequence.at(playerId) = sequence;
            std::vector<float> samples;
            if (mVoiceCodec.decode(encoded, samples))
                mAudioSystem->playVoice(playerId, samples);
        }
    }
}

void CNetworkThread::receiveSnapshots(std::vector<std::uint8_t>& buffer) {
    asio::ip::udp::endpoint sender;
    for (;;) {
        asio::error_code error;
        const auto       size = mSocket.receive_from(asio::buffer(buffer), sender, 0, error);
        if (error == asio::error::would_block)
            break;
        if (error)
            break;
        if (Net::validPacket(buffer.data(), size, Net::ePacketType::SNAPSHOT)) {
            std::vector<Game::SEntityComponents> entities;
            std::uint64_t                        tick = 0;
            if (!Net::readSnapshot(buffer.data(), size, entities, tick) ||
                (mHasReceivedTick && tick < mLastReceivedTick))
                continue;
            std::scoped_lock lock(mtex);
            mSceneData.replaceEntities(std::move(entities));
            mSceneData.tick   = tick;
            mLastReceivedTick = tick;
            mHasReceivedTick  = true;
            mAuthoritativeState.store(true);
        } else if (Net::validPacket(buffer.data(), size, Net::ePacketType::UPDATE)) {
            std::vector<Game::SUpdateEntity> updates;
            std::uint64_t                    tick = 0;
            if (!Net::readUpdate(buffer.data(), size, updates, tick) ||
                (mHasReceivedTick && tick < mLastReceivedTick))
                continue;
            std::scoped_lock lock(mtex);
            if (mSceneData.applyUpdates(updates)) {
                mSceneData.tick   = tick;
                mLastReceivedTick = tick;
                mHasReceivedTick  = true;
                mAuthoritativeState.store(true);
            }
        }
    }
}
