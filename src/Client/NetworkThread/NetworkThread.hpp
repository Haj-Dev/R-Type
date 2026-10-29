#pragma once

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
#include "Shared/Systems/ChatSystem.hpp"

class CNetworkThread {
  public:
    CNetworkThread(asio::io_context& io, SSceneData& sceneData, SPLayerActions& playerActions,
                   std::mutex& sceneMutex, std::atomic_bool& running,
                   std::condition_variable& stopCondition, const std::string& masterAddress,
                   std::uint16_t masterPort, const std::string& preferredServer,
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
    enum class eSelectionResult : std::uint8_t {
        SELECTED,
        REFRESH,
        STOPPED,
    };

    struct SSelection {
        eSelectionResult   result = eSelectionResult::STOPPED;
        Lobby::SServerInfo server;
    };

    void                     run();
    bool                     discoverServers();
    SSelection               waitForServerSelection();
    bool                     connectToGameServer(const Lobby::SServerInfo& server);
    void                     runGameSession();
    void                     processLobbyCommands();
    void                     processLobbyMessages();
    void                     handleLobbyMessage(Net::eLobbyCommand command, const std::uint8_t* payload,
                                                std::size_t payloadSize);
    void                     sendLobbyFrame(const std::vector<std::uint8_t>& frame);
    void                     sendInput();
    void                     receiveSnapshots(std::vector<std::uint8_t>& buffer);

    asio::io_context&        mIo;
    SSceneData&              mSceneData;
    SPLayerActions&          mPlayerActions;
    std::mutex&              mtex;
    std::atomic_bool&        mRunning;
    std::condition_variable& mStopCondition;
    std::string              mMasterAddress;
    std::uint16_t            mMasterPort;
    std::string              mPreferredServer;
    Lobby::SLobbyState&      mLobbyState;
    std::deque<Lobby::SCommand>& mLobbyCommands;
    std::thread                  mThread;
    asio::ip::udp::socket        mSocket;
    asio::ip::tcp::socket        mTcpSocket;
    asio::ip::udp::endpoint      mServerEndpoint;
    std::uint8_t                 mPlayerId   = 0;
    std::uint8_t                 mGameplayId = 0;
    std::atomic_bool             mConnected  = false;
    std::vector<std::uint8_t>    mTcpBuffer;
    bool                         mInGame = false;
    Chat::CChatSystem            mChatSystem;
};
