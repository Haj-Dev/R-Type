#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "Client/NetworkThread/NetworkThread.hpp"
#include "Client/drawThread/DrawThread.hpp"
#include "Client/logicThread/LogicThread.hpp"
#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"

class CClient {
  public:
    CClient();
    ~CClient();

    CClient(const CClient&)                       = delete;
    CClient& operator=(const CClient&)            = delete;
    CClient(CClient&&)                            = delete;
    CClient&                 operator=(CClient&&) = delete;

    void                     start();
    void                     stop();

    [[nodiscard]] SSceneData sceneData() const;

  private:
    mutable std::mutex      mtex;
    std::condition_variable mStopCondition;
    std::atomic_bool        mRunning = false;
    SSceneData              mSceneData;
    SPLayerActions          mPlayerActions;
    CLogicThread            mLogicThread;
    CDrawThread             mDrawThread;
    CNetworkThread          mNetworkThread;
};