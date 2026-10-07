#include "NetworkThread.hpp"

#include <chrono>

CServerNetworkThread::CServerNetworkThread(asio::io_context& io, SSceneData& sceneData,
                                           SPLayerActions& playerActions, std::mutex& sceneMutex,
                                           std::atomic_bool&        running,
                                           std::condition_variable& stopCondition) :
    mIo(io), mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition) {}

CServerNetworkThread::~CServerNetworkThread() {
    join();
}

void CServerNetworkThread::start() {
    if (!mThread.joinable()) {
        mThread = std::thread(&CServerNetworkThread::run, this);
    }
}

void CServerNetworkThread::join() {
    if (mThread.joinable()) {
        mThread.join();
    }
}

void CServerNetworkThread::run() {
    constexpr auto networkInterval = std::chrono::milliseconds(1);

    while (mRunning.load()) {
        mIo.poll();

        std::unique_lock lock(mtex);
        if (mStopCondition.wait_for(lock, networkInterval, [this] { return !mRunning.load(); })) {
            break;
        }
    }
}
