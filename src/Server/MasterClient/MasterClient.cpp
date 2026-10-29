#include "MasterClient.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <utility>

CMasterClient::CMasterClient(asio::io_context& io, std::string masterAddress, std::uint16_t masterPort,
                             Net::Control::SRegister registration, SServerStatus& status,
                             std::mutex& statusMutex, std::atomic_bool& running,
                             std::condition_variable& stopCondition) :
    mIo(io), mMasterAddress(std::move(masterAddress)), mMasterPort(masterPort),
    mRegistration(std::move(registration)), mStatus(status), mStatusMutex(statusMutex),
    mRunning(running), mStopCondition(stopCondition), mSocket(io) {}

CMasterClient::~CMasterClient() {
    join();
}

void CMasterClient::start() {
    if (!mThread.joinable())
        mThread = std::thread(&CMasterClient::run, this);
}

void CMasterClient::join() {
    if (mThread.joinable())
        mThread.join();
}

void CMasterClient::run() {
    while (mRunning.load()) {
        if (!connectAndRegister()) {
            waitBackoff();
            continue;
        }
        sessionLoop();
        if (!mRunning.load())
            break;
        waitBackoff();
    }

    if (mRegistered) {
        static_cast<void>(sendFrame(Net::Control::makeUnregister()));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (std::chrono::steady_clock::now() < deadline) {
            if (!readFrames())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    asio::error_code error;
    if (mSocket.is_open() && mSocket.close(error)) {
        std::cerr << "[Server] Failed to close master socket: " << error.message() << '\n';
    }
}

bool CMasterClient::connectAndRegister() {
    asio::error_code  error;
    asio::ip::address address = asio::ip::make_address(mMasterAddress, error);
    if (error) {
        asio::ip::tcp::resolver resolver(mIo);
        const auto results = resolver.resolve(mMasterAddress, std::to_string(mMasterPort), error);
        if (error || results.empty())
            return false;
        address = results.begin()->endpoint().address();
    }

    asio::ip::tcp::socket socket(mIo);
    if (socket.connect(asio::ip::tcp::endpoint(address, mMasterPort), error)) {
        std::cerr << "[Server] Unable to reach master at " << mMasterAddress << ':' << mMasterPort
                  << ": " << error.message() << '\n';
        return false;
    }
    mSocket = std::move(socket);

    const auto frame = Net::Control::makeRegister(mRegistration);
    if (frame.empty()) {
        std::cerr << "[Server] Invalid registration data\n";
        return false;
    }
    asio::write(mSocket, asio::buffer(frame), error);
    if (error)
        return false;

    std::array<std::uint8_t, Net::WireHeaderSize> header{};
    asio::read(mSocket, asio::buffer(header), error);
    if (error)
        return false;
    Net::Control::eCommand command;
    std::uint16_t          payloadLength = 0;
    if (!Net::Control::readHeader(header.data(), header.size(), command, payloadLength))
        return false;
    std::vector<std::uint8_t> payload(payloadLength);
    if (payloadLength > 0) {
        asio::read(mSocket, asio::buffer(payload), error);
        if (error)
            return false;
    }
    if (command != Net::Control::eCommand::REGISTER_ACK)
        return false;
    Net::Control::SRegisterAck ack;
    if (!Net::Control::readRegisterAck(payload.data(), payload.size(), ack))
        return false;
    if (ack.result != Net::Control::eRegisterResult::ACCEPTED) {
        std::cerr << "[Server] Master rejected registration: " << ack.message << '\n';
        return false;
    }

    if (mSocket.non_blocking(true, error)) {
        std::cerr << "[Server] Failed to set master socket non-blocking: " << error.message() << '\n';
        return false;
    }
    mBuffer.clear();
    mRegistered = true;
    mBackoff    = std::chrono::milliseconds(1000);
    std::cout << "[Server] Registered with master at " << mMasterAddress << ':' << mMasterPort << " as '"
              << mRegistration.name << "'\n";
    return true;
}

void CMasterClient::sessionLoop() {
    while (mRunning.load() && mSocket.is_open()) {
        if (!readFrames())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    mRegistered = false;
    asio::error_code error;
    if (mSocket.is_open() && mSocket.close(error)) {
        std::cerr << "[Server] Failed to close master socket: " << error.message() << '\n';
    }
}

bool CMasterClient::readFrames() {
    std::array<std::uint8_t, 1024> buffer{};
    for (;;) {
        asio::error_code error;
        const auto       received = mSocket.receive(asio::buffer(buffer), 0, error);
        if (error == asio::error::would_block || error == asio::error::try_again)
            break;
        if (error)
            return false;
        if (received == 0)
            break;
        mBuffer.insert(mBuffer.end(), buffer.begin(), buffer.begin() + received);
    }

    std::size_t offset = 0;
    while (mBuffer.size() - offset >= Net::WireHeaderSize) {
        Net::Control::eCommand command;
        std::uint16_t          payloadLength = 0;
        if (!Net::Control::readHeader(mBuffer.data() + offset, mBuffer.size() - offset, command,
                                      payloadLength))
            return false;
        const auto frameSize = Net::WireHeaderSize + payloadLength;
        if (mBuffer.size() - offset < frameSize)
            break;
        handleCommand(command, mBuffer.data() + offset + Net::WireHeaderSize, payloadLength);
        if (!mSocket.is_open())
            return false;
        offset += frameSize;
    }
    if (offset > 0)
        mBuffer.erase(mBuffer.begin(), mBuffer.begin() + static_cast<std::ptrdiff_t>(offset));
    return true;
}

void CMasterClient::handleCommand(Net::Control::eCommand command, const std::uint8_t* payload,
                                  std::size_t payloadSize) {
    switch (command) {
        case Net::Control::eCommand::KEEP_ALIVE: {
            std::uint8_t sequence = 0;
            if (!Net::Control::readKeepAlive(payload, payloadSize, sequence))
                return;
            Net::Control::SStatus status;
            {
                std::scoped_lock lock(mStatusMutex);
                status.rooms            = mStatus.rooms;
                status.connectedPlayers = mStatus.connectedPlayers;
                status.maxPlayers       = mStatus.maxPlayers;
                status.joinableRooms    = mStatus.joinableRooms;
                status.state            = mStatus.state;
            }
            status.sequence = sequence;
            static_cast<void>(sendFrame(Net::Control::makeStatus(status)));
            return;
        }
        case Net::Control::eCommand::UNREGISTER_ACK: {
            Net::Control::SUnregisterAck ack;
            if (Net::Control::readUnregisterAck(payload, payloadSize, ack))
                std::cout << "[Server] Master unregistration: " << ack.message << '\n';
            mRegistered = false;
            asio::error_code error;
            if (mSocket.is_open() && mSocket.close(error)) {
                std::cerr << "[Server] Failed to close master socket: " << error.message() << '\n';
            }
            return;
        }
        case Net::Control::eCommand::ERROR: {
            Net::Control::SError error;
            if (Net::Control::readError(payload, payloadSize, error))
                std::cerr << "[Server] Master error: " << error.message << '\n';
            return;
        }
        default: return;
    }
}

bool CMasterClient::sendFrame(const std::vector<std::uint8_t>& frame) {
    if (frame.empty() || !mSocket.is_open())
        return false;
    asio::error_code blockingError;
    if (mSocket.non_blocking(false, blockingError)) {
        std::cerr << "[Server] Failed to set master socket to blocking mode: " << blockingError.message()
                  << '\n';
    }
    asio::error_code writeError;
    asio::write(mSocket, asio::buffer(frame), writeError);
    asio::error_code nonBlockingError;
    if (mSocket.non_blocking(true, nonBlockingError)) {
        std::cerr << "[Server] Failed to restore master socket non-blocking mode: "
                  << nonBlockingError.message() << '\n';
    }
    if (writeError) {
        std::cerr << "[Server] Failed to send to master: " << writeError.message() << '\n';
        asio::error_code closeError;
        if (mSocket.is_open() && mSocket.close(closeError)) {
            std::cerr << "[Server] Failed to close master socket: " << closeError.message() << '\n';
        }
        return false;
    }
    return true;
}

void CMasterClient::waitBackoff() {
    static std::mt19937                generator(std::random_device{}());
    std::uniform_int_distribution<int> jitterDistribution(0, 499);
    const auto                         jitter = std::chrono::milliseconds(jitterDistribution(generator));
    const auto                         deadline = std::chrono::steady_clock::now() + mBackoff + jitter;
    while (mRunning.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mBackoff = std::min(mBackoff * 2, std::chrono::milliseconds(30000));
}
