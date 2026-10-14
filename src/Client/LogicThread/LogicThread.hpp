#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/Systems/GameSystems.hpp"

class CLogicThread {
  public:
    CLogicThread(SSceneData& sceneData, SPLayerActions& playerActions, std::mutex& sceneMutex,
                 std::atomic_bool& running, std::condition_variable& stopCondition);
    ~CLogicThread();

    CLogicThread(const CLogicThread&)            = delete;
    CLogicThread& operator=(const CLogicThread&) = delete;

    void          start();
    void          join();

  private:
    void                     run();

    SSceneData&              mSceneData;
    std::mutex&              mtex;
    std::atomic_bool&        mRunning;
    std::condition_variable& mStopCondition;
    std::thread              mThread;
    Game::CAnimationSystem   mAnimationSystem;
};
