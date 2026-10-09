#include "Client.hpp"

CClient::CClient(const std::string& serverAddress) :
    mLogicThread(mSceneData, mPlayerActions, mtex, mRunning, mStopCondition),
    mDrawThread(mSceneData, mPlayerActions, mtex, mRunning, mStopCondition),
    mNetworkThread(mIo, mSceneData, mPlayerActions, mtex, mRunning, mStopCondition, serverAddress) {
    start();
}

CClient::~CClient() {
    stop();
}

void CClient::start() {
    bool expected = false;
    if (!mRunning.compare_exchange_strong(expected, true)) {
        return;
    }

    mNetworkThread.start();
    {
        std::unique_lock lock(mtex);
        mStopCondition.wait(lock, [this] { return !mRunning.load() || mNetworkThread.connected(); });
    }
    if (!mRunning.load()) {
        mNetworkThread.join();
        return;
    }
    mLogicThread.start();
    mDrawThread.start();
}

void CClient::stop() {
    bool expected = true;
    if (!mRunning.compare_exchange_strong(expected, false)) {
        return;
    }

    mStopCondition.notify_all();

    mLogicThread.join();
    mDrawThread.join();
    mNetworkThread.join();
}
SSceneData CClient::sceneData() const {
    std::scoped_lock lock(mtex);
    return mSceneData;
}
