#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <asio.hpp>

#include "Client/NetworkThread/NetworkThread.hpp"
#include "Client/DrawThread/DrawThread.hpp"
#include "Client/LogicThread/LogicThread.hpp"
#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"

class CClient {
  public:
    explicit CClient(const std::string& serverAddress);
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
    mutable std::mutex      mtex;
    asio::io_context        mIo;
    std::condition_variable mStopCondition;
    std::atomic_bool        mRunning = false;
    SSceneData              mSceneData;
    SPLayerActions          mPlayerActions;
    CLogicThread            mLogicThread;
    CDrawThread             mDrawThread;
    CNetworkThread          mNetworkThread;
};
