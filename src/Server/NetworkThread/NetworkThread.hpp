#pragma once

#include <asio.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"

class CServerNetworkThread {
  public:
    CServerNetworkThread(asio::io_context& io, SSceneData& sceneData, SPLayerActions& playerActions,
                         std::mutex& sceneMutex, std::atomic_bool& running,
                         std::condition_variable& stopCondition);
    ~CServerNetworkThread();

    CServerNetworkThread(const CServerNetworkThread&)            = delete;
    CServerNetworkThread& operator=(const CServerNetworkThread&) = delete;

    void                  start();
    void                  join();

  private:
    void                     run();

    asio::io_context&        mIo;
    SSceneData&              mSceneData;
    SPLayerActions&          mPlayerActions;
    std::mutex&              mtex;
    std::atomic_bool&        mRunning;
    std::condition_variable& mStopCondition;
    std::thread              mThread;
};
