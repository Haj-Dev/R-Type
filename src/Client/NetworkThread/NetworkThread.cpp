#include "NetworkThread.hpp"

#include <chrono>

CNetworkThread::CNetworkThread(SSceneData& sceneData, SPLayerActions& playerActions,
                               std::mutex& sceneMutex, std::atomic_bool& running,
                               std::condition_variable& stopCondition) :
    mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition) {}

CNetworkThread::~CNetworkThread() {
    join();
}

void CNetworkThread::start() {
    if (!mThread.joinable()) {
        mThread = std::thread(&CNetworkThread::run, this);
    }
}

void CNetworkThread::join() {
    if (mThread.joinable()) {
        mThread.join();
    }
}

void CNetworkThread::run() {
    constexpr auto networkInterval = std::chrono::milliseconds(10);

    while (mRunning.load()) {
        std::unique_lock lock(mtex);
        if (mStopCondition.wait_for(lock, networkInterval, [this] { return !mRunning.load(); })) {
            break;
        }
    }
}
