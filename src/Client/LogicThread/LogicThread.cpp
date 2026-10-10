#include "LogicThread.hpp"

#include <chrono>

CLogicThread::CLogicThread(SSceneData& sceneData, SPLayerActions& playerActions, std::mutex& sceneMutex,
                           std::atomic_bool& running, std::condition_variable& stopCondition) :
    mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
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
    constexpr auto tickInterval = std::chrono::milliseconds(16);

    while (mRunning.load()) {
        std::unique_lock lock(mtex);
        if (mStopCondition.wait_for(lock, tickInterval, [this] { return !mRunning.load(); })) {
            break;
        }
    }
}
