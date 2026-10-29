#pragma once

#include <asio.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include "Master/MasterRegistry.hpp"
#include "Shared/ControlProtocol.hpp"

class CMasterServer {
  public:
    explicit CMasterServer(asio::io_context& io, std::uint16_t port = Net::Control::DefaultMasterPort);
    ~CMasterServer();

    CMasterServer(const CMasterServer&)            = delete;
    CMasterServer& operator=(const CMasterServer&) = delete;

    void           start();
    void           stop();

  private:
    struct SConnection {
        Master::ConnectionId      id = 0;
        asio::ip::tcp::socket     socket;
        std::vector<std::uint8_t> buffer;
        bool                      registered = false;
        bool                      closed     = false;
    };

    void                    run();
    void                    acceptConnections();
    void                    processConnection(SConnection& connection);
    void                    handleCommand(SConnection& connection, Net::Control::eCommand command,
                                          const std::uint8_t* payload, std::size_t payloadSize);
    void                    sendFrame(SConnection& connection, const std::vector<std::uint8_t>& frame);
    void                    closeConnection(SConnection& connection);
    void                    healthCheck();

    asio::io_context&       mIo;
    std::uint16_t           mPort;
    asio::ip::tcp::acceptor mAcceptor;
    std::thread             mThread;
    std::atomic_bool        mRunning = false;
    std::condition_variable mStopCondition;
    std::mutex              mMutex;
    std::list<SConnection>  mConnections;
    Master::CMasterRegistry mRegistry;
    Master::ConnectionId    mNextConnectionId = 1;
    std::chrono::steady_clock::time_point mLastHealthCheck;
};
