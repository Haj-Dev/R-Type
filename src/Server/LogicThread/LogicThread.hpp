#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"

class CServerLogicThread {
  public:
    CServerLogicThread(SSceneData& sceneData, SPLayerActions& playerActions, std::mutex& sceneMutex,
                       std::atomic_bool& running, std::condition_variable& stopCondition);
    ~CServerLogicThread();

    CServerLogicThread(const CServerLogicThread&)            = delete;
    CServerLogicThread& operator=(const CServerLogicThread&) = delete;

    void                start();
    void                join();

  private:
    void                     run();

    SSceneData&              mSceneData;
    SPLayerActions&          mPlayerActions;
    std::mutex&              mtex;
    std::atomic_bool&        mRunning;
    std::condition_variable& mStopCondition;
    std::thread              mThread;
};
