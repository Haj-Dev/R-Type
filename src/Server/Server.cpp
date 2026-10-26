#include "Server.hpp"

CServer::CServer(asio::io_context& io) :
    mIo(io), mLogicThread(mSceneData, mPlayerActions, mtex, mRunning, mStopCondition, mGameActive),
    mNetworkThread(mIo, mSceneData, mPlayerActions, mtex, mRunning, mStopCondition, mGameActive) {
    start();
}

CServer::~CServer() {
    stop();
}

void CServer::start() {
    bool expected = false;
    if (!mRunning.compare_exchange_strong(expected, true)) {
        return;
    }

    mLogicThread.start();
    mNetworkThread.start();
}

void CServer::stop() {
    bool expected = true;
    if (!mRunning.compare_exchange_strong(expected, false)) {
        return;
    }

    mStopCondition.notify_all();
    mIo.stop();
    mLogicThread.join();
    mNetworkThread.join();
}

SSceneData CServer::sceneData() const {
    std::scoped_lock lock(mtex);
    return mSceneData;
}
