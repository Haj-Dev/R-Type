#pragma once

#include <array>
#include <asio.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/Types.hpp"

class CServerNetworkThread {
  public:
    CServerNetworkThread(asio::io_context& io, SSceneData& sceneData, SPLayerActions& playerActions,
                         std::mutex& sceneMutex, std::atomic_bool& running,
                         std::condition_variable& stopCondition);
    ~CServerNetworkThread();

    CServerNetworkThread(const CServerNetworkThread&)            = delete;
    CServerNetworkThread& operator=(const CServerNetworkThread&) = delete;

    void                  start();
    void                  join();

  private:
    void                                 run();

    asio::io_context&                    mIo;
    SSceneData&                          mSceneData;
    SPLayerActions&                      mPlayerActions;
    std::mutex&                          mtex;
    std::atomic_bool&                    mRunning;
    std::condition_variable&             mStopCondition;
    std::thread                          mThread;
    asio::ip::udp::socket                mSocket;
    asio::ip::tcp::acceptor              mAcceptor;
    std::vector<asio::ip::tcp::socket>   mControlSockets;
    std::vector<asio::ip::udp::endpoint> mClients;

    struct SPendingHandshake {
        asio::ip::tcp::socket                        socket;
        std::array<std::uint8_t, Net::HandshakeSize> data{};
        std::size_t                                  received = 0;
    };
    std::vector<SPendingHandshake> mPending;
};
