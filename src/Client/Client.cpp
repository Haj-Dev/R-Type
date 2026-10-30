#include "Client.hpp"

CClient::CClient(const std::string& masterAddress, std::uint16_t masterPort,
                 const std::string& preferredServer) :
    mLogicThread(mSceneData, mPlayerActions, mtex, mRunning, mStopCondition),
    mDrawThread(mSceneData, mPlayerActions, mtex, mRunning, mStopCondition, mLobbyState, mLobbyCommands),
    mNetworkThread(mIo, mSceneData, mPlayerActions, mtex, mRunning, mStopCondition, masterAddress,
                   masterPort, preferredServer, mLobbyState, mLobbyCommands) {
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

    mLogicThread.start();
    mDrawThread.start();
    mNetworkThread.start();
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
