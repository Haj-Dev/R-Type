#pragma once

#include <atomic>
#include <asio.hpp>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"

class CNetworkThread {
  public:
    CNetworkThread(asio::io_context& io, SSceneData& sceneData, SPLayerActions& playerActions,
                   std::mutex& sceneMutex, std::atomic_bool& running,
                   std::condition_variable& stopCondition, const std::string& serverAddress);
    ~CNetworkThread();

    CNetworkThread(const CNetworkThread&)               = delete;
    CNetworkThread&    operator=(const CNetworkThread&) = delete;

    void               start();
    void               join();
    [[nodiscard]] bool connected() const {
        return mConnected.load();
    }

  private:
    void                     run();

    SSceneData&              mSceneData;
    SPLayerActions&          mPlayerActions;
    std::mutex&              mtex;
    std::atomic_bool&        mRunning;
    std::condition_variable& mStopCondition;
    std::thread              mThread;
    asio::io_context&        mIo;
    asio::ip::udp::socket    mSocket;
    asio::ip::tcp::socket    mTcpSocket;
    asio::ip::udp::endpoint  mServerEndpoint;
    std::uint8_t             mPlayerId  = 0;
    std::atomic_bool         mConnected = false;
};
