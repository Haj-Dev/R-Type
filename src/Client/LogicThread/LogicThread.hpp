#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/GameSimulation.hpp"
#include "Shared/Lobby.hpp"

class CLogicThread {
  public:
    CLogicThread(SSceneData& sceneData, SPLayerActions& playerActions, Lobby::SLobbyState& lobbyState,
                 std::atomic_bool& authoritativeState, std::mutex& sceneMutex, std::atomic_bool& running,
                 std::condition_variable& stopCondition);
    ~CLogicThread();

    CLogicThread(const CLogicThread&)            = delete;
    CLogicThread& operator=(const CLogicThread&) = delete;

    void          start();
    void          join();

  private:
    void                     run();
    void                     updateLocalSimulation(float delta);

    SSceneData&              mSceneData;
    SPLayerActions&          mPlayerActions;
    Lobby::SLobbyState&      mLobbyState;
    std::atomic_bool&        mAuthoritativeState;
    std::mutex&              mtex;
    std::atomic_bool&        mRunning;
    std::condition_variable& mStopCondition;
    std::thread              mThread;
    Game::CSimulation        mSimulation;
};
