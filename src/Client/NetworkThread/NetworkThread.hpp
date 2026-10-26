#pragma once

#include <array>
#include <atomic>
#include <asio.hpp>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Shared/Lobby.hpp"
#include "Shared/LobbyProtocol.hpp"
#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"

class CNetworkThread {
  public:
    CNetworkThread(asio::io_context& io, SSceneData& sceneData, SPLayerActions& playerActions,
                   std::mutex& sceneMutex, std::atomic_bool& running,
                   std::condition_variable& stopCondition, const std::string& serverAddress,
                   Lobby::SLobbyState& lobbyState, std::deque<Lobby::SCommand>& lobbyCommands);
    ~CNetworkThread();

    CNetworkThread(const CNetworkThread&)               = delete;
    CNetworkThread&    operator=(const CNetworkThread&) = delete;

    void               start();
    void               join();
    [[nodiscard]] bool connected() const {
        return mConnected.load();
    }

  private:
    void                     run();
    void                     processLobbyCommands();
    void                     processLobbyMessages();
    void                     handleLobbyMessage(Net::eLobbyCommand command, const std::uint8_t* payload,
                                                std::size_t payloadSize);
    void                     sendLobbyFrame(const std::vector<std::uint8_t>& frame);
    void                     sendInput();
    void                     receiveSnapshots(std::vector<std::uint8_t>& buffer);

    SSceneData&              mSceneData;
    SPLayerActions&          mPlayerActions;
    std::mutex&              mtex;
    std::atomic_bool&        mRunning;
    std::condition_variable& mStopCondition;
    Lobby::SLobbyState&      mLobbyState;
    std::deque<Lobby::SCommand>& mLobbyCommands;
    std::thread                  mThread;
    asio::ip::udp::socket        mSocket;
    asio::ip::tcp::socket        mTcpSocket;
    asio::ip::udp::endpoint      mServerEndpoint;
    std::uint8_t                 mPlayerId  = 0;
    std::atomic_bool             mConnected = false;
    std::vector<std::uint8_t>    mTcpBuffer;
    bool                         mInGame = false;
};
