#pragma once

#include <asio.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "Server/NetworkThread/NetworkThread.hpp"
#include "Server/LogicThread/LogicThread.hpp"
#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"

class CServer {
  public:
    explicit CServer(asio::io_context& io);
    ~CServer();

    CServer(const CServer&)                       = delete;
    CServer& operator=(const CServer&)            = delete;
    CServer(CServer&&)                            = delete;
    CServer&                 operator=(CServer&&) = delete;

    void                     start();
    void                     stop();

    [[nodiscard]] SSceneData sceneData() const;

  private:
    asio::io_context&       mIo;
    mutable std::mutex      mtex;
    std::condition_variable mStopCondition;
    std::atomic_bool        mRunning = false;
    SSceneData              mSceneData;
    SPLayerActions          mPlayerActions;
    CServerLogicThread      mLogicThread;
    CServerNetworkThread    mNetworkThread;
};
