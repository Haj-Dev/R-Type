#include "Server.hpp"

#include <utility>

CServer::CServer(asio::io_context& io, std::uint16_t tcpPort, std::uint16_t udpPort,
                 std::optional<SMasterConfig> masterConfig) :
    mIo(io), mLogicThread(mSceneData, mPlayerActions, mtex, mRunning, mStopCondition, mGameActive),
    mNetworkThread(mIo, mSceneData, mPlayerActions, mtex, mRunning, mStopCondition, mGameActive, tcpPort,
                   udpPort, mServerStatus, mStatusMutex) {
    if (masterConfig) {
        Net::Control::SRegister registration{
            .name       = masterConfig->serverName,
            .tcpPort    = tcpPort,
            .udpPort    = udpPort,
            .maxPlayers = masterConfig->maxPlayers,
            .major      = Net::Control::ProtocolMajor,
            .minor      = Net::Control::ProtocolMinor,
            .address    = masterConfig->advertisedAddress,
        };
        mMasterClient = std::make_unique<CMasterClient>(
            mIo, masterConfig->masterAddress, masterConfig->masterPort, std::move(registration),
            mServerStatus, mStatusMutex, mRunning, mStopCondition);
    }
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
    if (mMasterClient)
        mMasterClient->start();
}

void CServer::stop() {
    bool expected = true;
    if (!mRunning.compare_exchange_strong(expected, false)) {
        return;
    }

    mStopCondition.notify_all();
    mIo.stop();
    if (mMasterClient)
        mMasterClient->join();
    mLogicThread.join();
    mNetworkThread.join();
}

SSceneData CServer::sceneData() const {
    std::scoped_lock lock(mtex);
    return mSceneData;
}
