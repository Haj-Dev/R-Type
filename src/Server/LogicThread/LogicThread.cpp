#include "LogicThread.hpp"

#include "Shared/FixedRate.hpp"

#include <chrono>

CServerLogicThread::CServerLogicThread(SSceneData& sceneData, SPLayerActions& playerActions,
                                       std::mutex& sceneMutex, std::atomic_bool& running,
                                       std::condition_variable& stopCondition) :
    mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition) {}

CServerLogicThread::~CServerLogicThread() {
    join();
}

void CServerLogicThread::start() {
    if (!mThread.joinable()) {
        mThread = std::thread(&CServerLogicThread::run, this);
    }
}

void CServerLogicThread::join() {
    if (mThread.joinable()) {
        mThread.join();
    }
}

void CServerLogicThread::run() {
    constexpr auto  tickInterval = std::chrono::nanoseconds(1'000'000'000 / 60);
    constexpr float tickDelta    = 1.0F / 60.0F;

    const Timing::CScopedTimerResolution timerResolution;
    Timing::CFixedRate                   tickRate(tickInterval);

    while (mRunning.load()) {
        {
            std::scoped_lock lock(mtex);
            mSimulation.applyActions(mPlayerActions);
            mSimulation.tick(tickDelta);
            mSceneData.entities = mSimulation.snapshot();
            ++mSceneData.tick;
        }

        std::unique_lock lock(mtex);
        if (!tickRate.wait(mStopCondition, lock, [this] { return !mRunning.load(); }))
            break;
    }
}
