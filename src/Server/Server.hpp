#pragma once

#include <asio.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "Server/LogicThread/LogicThread.hpp"
#include "Server/MasterClient/MasterClient.hpp"
#include "Server/NetworkThread/NetworkThread.hpp"
#include "Server/ServerStatus.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/SceneData.hpp"

struct SMasterConfig {
    std::string   masterAddress;
    std::uint16_t masterPort = Net::Control::DefaultMasterPort;
    std::string   serverName;
    std::string   advertisedAddress;
    std::uint16_t maxPlayers = Room::MaxConnections;
};

class CServer {
  public:
    explicit CServer(asio::io_context& io, std::uint16_t tcpPort = Net::DefaultTcpPort,
                     std::uint16_t                udpPort      = Net::DefaultUdpPort,
                     std::optional<SMasterConfig> masterConfig = std::nullopt);
    ~CServer();

    CServer(const CServer&)                       = delete;
    CServer& operator=(const CServer&)            = delete;
    CServer(CServer&&)                            = delete;
    CServer&                 operator=(CServer&&) = delete;

    void                     start();
    void                     stop();

    [[nodiscard]] SSceneData sceneData() const;

  private:
    asio::io_context&              mIo;
    mutable std::mutex             mtex;
    std::condition_variable        mStopCondition;
    std::atomic_bool               mRunning    = false;
    std::atomic_bool               mGameActive = false;
    SSceneData                     mSceneData;
    SPLayerActions                 mPlayerActions;
    SServerStatus                  mServerStatus;
    std::mutex                     mStatusMutex;
    CServerLogicThread             mLogicThread;
    CServerNetworkThread           mNetworkThread;
    std::unique_ptr<CMasterClient> mMasterClient;
};
