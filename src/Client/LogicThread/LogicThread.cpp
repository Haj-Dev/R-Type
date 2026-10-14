#include "LogicThread.hpp"

#include <chrono>

CLogicThread::CLogicThread(SSceneData& sceneData, SPLayerActions&, std::mutex& sceneMutex,
                           std::atomic_bool& running, std::condition_variable& stopCondition) :
    mSceneData(sceneData), mtex(sceneMutex), mRunning(running), mStopCondition(stopCondition) {}

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
    constexpr auto  tickInterval = std::chrono::milliseconds(16);
    constexpr float tickDelta    = 0.016F;

    while (mRunning.load()) {
        {
            std::scoped_lock lock(mtex);
            mAnimationSystem.update(mSceneData.renderRegistry, tickDelta);
        }
        std::unique_lock lock(mtex);
        if (mStopCondition.wait_for(lock, tickInterval, [this] { return !mRunning.load(); })) {
            break;
        }
    }
}
