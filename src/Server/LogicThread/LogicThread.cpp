#include "LogicThread.hpp"

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
    constexpr auto tickInterval = std::chrono::milliseconds(16);

    while (mRunning.load()) {
        {
            std::scoped_lock lock(mtex);
            mSimulation.applyActions(mPlayerActions);
            mSimulation.tick(0.016F);
            mSceneData.entities = mSimulation.snapshot();
            ++mSceneData.tick;
        }
        std::unique_lock lock(mtex);
        if (mStopCondition.wait_for(lock, tickInterval, [this] { return !mRunning.load(); })) {
            break;
        }
    }
}
