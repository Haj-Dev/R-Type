#pragma once

#include <asio.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Server/ServerStatus.hpp"
#include "Shared/ControlProtocol.hpp"

class CMasterClient {
  public:
    CMasterClient(asio::io_context& io, std::string masterAddress, std::uint16_t masterPort,
                  Net::Control::SRegister registration, SServerStatus& status, std::mutex& statusMutex,
                  std::atomic_bool& running, std::condition_variable& stopCondition);
    ~CMasterClient();

    CMasterClient(const CMasterClient&)            = delete;
    CMasterClient& operator=(const CMasterClient&) = delete;

    void           start();
    void           join();

  private:
    void                      run();
    bool                      connectAndRegister();
    void                      sessionLoop();
    bool                      readFrames();
    void                      handleCommand(Net::Control::eCommand command, const std::uint8_t* payload,
                                            std::size_t payloadSize);
    bool                      sendFrame(const std::vector<std::uint8_t>& frame);
    void                      waitBackoff();

    asio::io_context&         mIo;
    std::string               mMasterAddress;
    std::uint16_t             mMasterPort;
    Net::Control::SRegister   mRegistration;
    SServerStatus&            mStatus;
    std::mutex&               mStatusMutex;
    std::atomic_bool&         mRunning;
    std::condition_variable&  mStopCondition;
    std::thread               mThread;
    asio::ip::tcp::socket     mSocket;
    std::vector<std::uint8_t> mBuffer;
    bool                      mRegistered = false;
    std::chrono::milliseconds mBackoff{1000};
};
