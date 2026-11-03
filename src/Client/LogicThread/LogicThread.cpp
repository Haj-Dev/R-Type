#include "LogicThread.hpp"

#include "Shared/FixedRate.hpp"

#include <chrono>

CLogicThread::CLogicThread(SSceneData& sceneData, SPLayerActions& playerActions,
                           Lobby::SLobbyState& lobbyState, std::atomic_bool& authoritativeState,
                           std::mutex& sceneMutex, std::atomic_bool& running,
                           std::condition_variable& stopCondition) :
    mSceneData(sceneData), mPlayerActions(playerActions), mLobbyState(lobbyState),
    mAuthoritativeState(authoritativeState), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition) {}

CLogicThread::~CLogicThread() {
    join();
}

void CLogicThread::start() {
    if (!mThread.joinable()) {
        mThread = std::thread(&CLogicThread::run, this);
    }
}

void CLogicThread::join() {
    if (mThread.joinable()) {
        mThread.join();
    }
}

void CLogicThread::run() {
    constexpr auto                       tickInterval = std::chrono::nanoseconds(1'000'000'000 / 60);
    constexpr float                      tickDelta    = 1.0F / 60.0F;
    const Timing::CScopedTimerResolution timerResolution;
    Timing::CFixedRate                   tickRate(tickInterval);
    bool                                 wasInGame = false;

    while (mRunning.load()) {
        {
            std::scoped_lock lock(mtex);
            const bool       inGame = mLobbyState.phase == Lobby::eLobbyPhase::IN_GAME;
            if (inGame && !wasInGame)
                mSimulation.reset();
            const bool serverStateReceived = mAuthoritativeState.exchange(false);
            if (inGame && serverStateReceived) {
                mSimulation.synchronize(mSceneData.entities, mLobbyState.playerId);
            } else if (inGame) {
                updateLocalSimulation(tickDelta);
            }
            wasInGame = inGame;
        }
        std::unique_lock lock(mtex);
        if (!tickRate.wait(mStopCondition, lock, [this] { return !mRunning.load(); }))
            break;
    }
}

void CLogicThread::updateLocalSimulation(float delta) {
    SPLayerActions localActions;
    const auto     localPlayerId = mLobbyState.playerId;
    if (localPlayerId < SPLayerActions::MaxPlayers) {
        localActions.connected.at(localPlayerId) = true;
        localActions.states.at(localPlayerId)    = mPlayerActions.states.at(localPlayerId);
    }
    mSimulation.applyActions(localActions);
    mSimulation.tick(delta, false);
    mSceneData.replaceEntities(mSimulation.snapshot());
    ++mSceneData.tick;
}
