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
#include "Shared/LobbyProtocol.hpp"
#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/Types.hpp"

class CServerNetworkThread {
  public:
    CServerNetworkThread(asio::io_context& io, SSceneData& sceneData, SPLayerActions& playerActions,
                         std::mutex& sceneMutex, std::atomic_bool& running,
                         std::condition_variable& stopCondition, std::atomic_bool& gameActive);
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
    void disconnectPlayer(std::uint8_t playerId);

    asio::io_context&                                                 mIo;
    SSceneData&                                                       mSceneData;
    SPLayerActions&                                                   mPlayerActions;
    std::mutex&                                                       mtex;
    std::atomic_bool&                                                 mRunning;
    std::condition_variable&                                          mStopCondition;
    std::atomic_bool&                                                 mGameActive;
    std::thread                                                       mThread;
    asio::ip::udp::socket                                             mSocket;
    asio::ip::tcp::acceptor                                           mAcceptor;
    std::vector<asio::ip::tcp::socket>                                mControlSockets;
    std::vector<asio::ip::udp::endpoint>                              mClients;
    std::array<std::vector<std::uint8_t>, SPLayerActions::MaxPlayers> mTcpBuffers;
    CRoomManager                                                      mRoomManager;
    std::uint64_t                                                     mLastSentTick = 0;
    bool                                                               mHasSentTick = false;

    struct SPendingHandshake {
        asio::ip::tcp::socket                        socket;
        std::array<std::uint8_t, Net::HandshakeSize> data{};
        std::size_t                                  received = 0;
    };
    std::vector<SPendingHandshake> mPending;
};
