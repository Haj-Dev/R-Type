#pragma once

#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/Lobby.hpp"
#include "Shared/ControlProtocol.hpp"

#include "Client/DrawThread/DrawThread.hpp"
#include "Client/LogicThread/LogicThread.hpp"
#include "Client/NetworkThread/NetworkThread.hpp"

#include <asio.hpp>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

class CClient {
  public:
    explicit CClient(const std::string& masterAddress,
                     std::uint16_t      masterPort      = Net::Control::DefaultMasterPort,
                     const std::string& preferredServer = {});
    ~CClient();

    CClient(const CClient&)                 = delete;
    CClient& operator=(const CClient&)      = delete;
    CClient(CClient&&)                      = delete;
    CClient&           operator=(CClient&&) = delete;

    void               start();
    void               stop();
    [[nodiscard]] bool running() const {
        return mRunning.load();
    }

    [[nodiscard]] SSceneData sceneData() const;

  private:
    mutable std::mutex          mtex;
    asio::io_context            mIo;
    std::condition_variable     mStopCondition;
    std::atomic_bool            mRunning = false;
    SSceneData                  mSceneData;
    SPLayerActions              mPlayerActions;
    Lobby::SLobbyState          mLobbyState;
    std::deque<Lobby::SCommand> mLobbyCommands;
    CLogicThread                mLogicThread;
    CDrawThread                 mDrawThread;
    CNetworkThread              mNetworkThread;
};
