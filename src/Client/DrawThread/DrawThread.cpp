#include "DrawThread.hpp"

#include <chrono>

CDrawThread::CDrawThread(SSceneData& sceneData, SPLayerActions& playerActions, std::mutex& sceneMutex,
                         std::atomic_bool& running, std::condition_variable& stopCondition) :
    mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition) {}

CDrawThread::~CDrawThread() {
    join();
}

void CDrawThread::start() {
    if (!mThread.joinable()) {
        mThread = std::thread(&CDrawThread::run, this);
    }
}

void CDrawThread::join() {
    if (mThread.joinable()) {
        mThread.join();
    }
}

void CDrawThread::run() {
    constexpr auto drawInterval = std::chrono::milliseconds(16);

    while (mRunning.load()) {
        std::unique_lock lock(mtex);
        if (mStopCondition.wait_for(lock, drawInterval, [this] { return !mRunning.load(); })) {
            break;
        }
    }
}
