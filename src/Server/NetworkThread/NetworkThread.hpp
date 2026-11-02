#pragma once

#include <array>
#include <asio.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "Server/Room/RoomManager.hpp"
#include "Server/ServerStatus.hpp"
#include "Shared/LobbyProtocol.hpp"
#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/Types.hpp"

class CServerNetworkThread {
  public:
    CServerNetworkThread(asio::io_context& io, SSceneData& sceneData, SPLayerActions& playerActions,
                         std::mutex& sceneMutex, std::atomic_bool& running,
                         std::condition_variable& stopCondition, std::atomic_bool& gameActive,
                         std::uint16_t tcpPort, std::uint16_t udpPort, std::uint16_t voicePort,
                         SServerStatus& serverStatus, std::mutex& statusMutex);
    ~CServerNetworkThread();

    CServerNetworkThread(const CServerNetworkThread&)            = delete;
    CServerNetworkThread& operator=(const CServerNetworkThread&) = delete;

    void                  start();
    void                  join();

  private:
    void run();

    void processLobby(std::uint8_t playerId);
    void handleLobbyCommand(std::uint8_t playerId, Net::eLobbyCommand command,
                            const std::uint8_t* payload, std::size_t payloadSize);
    void sendFrame(std::uint8_t playerId, const std::vector<std::uint8_t>& frame);
    void sendRoomList(std::uint8_t playerId);
    void sendRoomInfo(std::uint8_t playerId, Net::eLobbyCommand command, const Room::SInfo& room);
    void sendRejection(std::uint8_t playerId, Room::eRejectReason reason, const std::string& message);
    void broadcastRoom(std::uint16_t roomId, const std::vector<std::uint8_t>& frame,
                       std::optional<std::uint8_t> except = std::nullopt);
    [[nodiscard]] bool spectatingActiveGame(std::uint8_t playerId) const;
    // Tells spectators whose room closed (its last player left) that they are back in the lobby.
    void releaseOrphanedSpectators(const std::vector<std::uint8_t>& spectators, std::uint8_t except);
    void disconnectPlayer(std::uint8_t playerId);
    void updateServerStatus();

    asio::io_context&                                             mIo;
    SSceneData&                                                   mSceneData;
    SPLayerActions&                                               mPlayerActions;
    std::mutex&                                                   mtex;
    std::atomic_bool&                                             mRunning;
    std::condition_variable&                                      mStopCondition;
    std::atomic_bool&                                             mGameActive;
    std::uint16_t                                                 mTcpPort;
    std::uint16_t                                                 mUdpPort;
    std::uint16_t                                                 mVoicePort;
    SServerStatus&                                                mServerStatus;
    std::mutex&                                                   mStatusMutex;
    std::thread                                                   mThread;
    asio::ip::udp::socket                                         mSocket;
    asio::ip::udp::socket                                         mVoiceSocket;
    asio::ip::tcp::acceptor                                       mAcceptor;
    std::vector<asio::ip::tcp::socket>                            mControlSockets;
    std::vector<asio::ip::udp::endpoint>                          mClients;
    std::vector<asio::ip::udp::endpoint>                          mVoiceClients;
    std::array<std::vector<std::uint8_t>, Room::MaxConnections>   mTcpBuffers;
    std::array<std::optional<std::uint8_t>, Room::MaxConnections> mConnectionGameplayIds;
    CRoomManager                                                  mRoomManager;
    std::uint32_t                                                 mChatSequence = 0;

    struct SPendingHandshake {
        asio::ip::tcp::socket                        socket;
        std::array<std::uint8_t, Net::HandshakeSize> data{};
        std::size_t                                  received = 0;
    };
    std::vector<SPendingHandshake> mPending;
};
