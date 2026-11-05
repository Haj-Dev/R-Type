#include "MasterServer.hpp"

#include <array>
#include <chrono>
#include <iostream>

namespace {

    constexpr auto HealthCheckInterval = std::chrono::milliseconds(1000);
    constexpr auto ResponseDeadline    = std::chrono::milliseconds(1000);
    constexpr auto UnhealthyTimeout    = std::chrono::milliseconds(3000);
    constexpr auto LoopInterval        = std::chrono::milliseconds(10);

} // namespace

CMasterServer::CMasterServer(asio::io_context& io, std::uint16_t port) :
    mIo(io), mPort(port), mAcceptor(io) {}

CMasterServer::~CMasterServer() {
    stop();
}

void CMasterServer::start() {
    bool expected = false;
    if (!mRunning.compare_exchange_strong(expected, true))
        return;
    mThread = std::thread(&CMasterServer::run, this);
}

void CMasterServer::stop() {
    bool expected = true;
    if (!mRunning.compare_exchange_strong(expected, false))
        return;
    mStopCondition.notify_all();
    asio::error_code error;
    if (mAcceptor.is_open() && mAcceptor.close(error)) {
        std::cerr << "[Master] Failed to close acceptor: " << error.message() << '\n';
    }
    if (mThread.joinable())
        mThread.join();
}

void CMasterServer::run() {
    asio::error_code error;
    if (mAcceptor.open(asio::ip::tcp::v4(), error)) {
        std::cerr << "[Master] Unable to open acceptor: " << error.message() << '\n';
        mRunning.store(false);
        return;
    }
    if (mAcceptor.set_option(asio::socket_base::reuse_address(true), error)) {
        std::cerr << "[Master] Unable to set reuse_address: " << error.message() << '\n';
        mRunning.store(false);
        return;
    }
    if (mAcceptor.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), mPort), error)) {
        std::cerr << "[Master] Unable to bind port " << mPort << ": " << error.message() << '\n';
        mRunning.store(false);
        return;
    }
    if (mAcceptor.listen(asio::socket_base::max_listen_connections, error)) {
        std::cerr << "[Master] Unable to listen: " << error.message() << '\n';
        mRunning.store(false);
        return;
    }
    if (mAcceptor.non_blocking(true, error)) {
        std::cerr << "[Master] Unable to set non-blocking mode: " << error.message() << '\n';
        mRunning.store(false);
        return;
    }
    std::cout << "[Master] Listening on TCP port " << mPort << '\n';
    mLastHealthCheck = std::chrono::steady_clock::now();

    while (mRunning.load()) {
        acceptConnections();

        for (auto connection = mConnections.begin(); connection != mConnections.end();) {
            processConnection(*connection);
            if (connection->closed)
                connection = mConnections.erase(connection);
            else
                ++connection;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - mLastHealthCheck >= HealthCheckInterval) {
            mLastHealthCheck = now;
            healthCheck();
        }

        std::unique_lock lock(mMutex);
        if (mStopCondition.wait_for(lock, LoopInterval, [this] { return !mRunning.load(); }))
            break;
    }

    for (auto& connection : mConnections) {
        mRegistry.removeConnection(connection.id);
        asio::error_code closeError;
        if (connection.socket.close(closeError)) {
            std::cerr << "[Master] Failed to close connection socket: " << closeError.message() << '\n';
        }
    }
    mConnections.clear();
}

void CMasterServer::acceptConnections() {
    for (;;) {
        asio::error_code      error;
        asio::ip::tcp::socket socket(mIo);
        const auto            result = mAcceptor.accept(socket, error);
        if (result)
            error = result;
        if (error == asio::error::would_block || error == asio::error::try_again)
            break;
        if (error) {
            std::cerr << "[Master] Accept error: " << error.message() << '\n';
            break;
        }
        if (socket.non_blocking(true, error)) {
            std::cerr << "[Master] Unable to set client socket non-blocking: " << error.message()
                      << '\n';
            break;
        }
        mConnections.push_back(SConnection{
            .id         = mNextConnectionId++,
            .socket     = std::move(socket),
            .buffer     = {},
            .registered = false,
            .closed     = false,
        });
        std::cout << "[Master] Connection " << mConnections.back().id << " accepted\n";
    }
}

void CMasterServer::processConnection(SConnection& connection) {
    std::array<std::uint8_t, 1024> buffer{};
    for (;;) {
        asio::error_code error;
        const auto       received = connection.socket.receive(asio::buffer(buffer), 0, error);
        if (error == asio::error::would_block || error == asio::error::try_again)
            break;
        if (error) {
            closeConnection(connection);
            return;
        }
        if (received == 0)
            break;
        connection.buffer.insert(connection.buffer.end(), buffer.begin(), buffer.begin() + received);
    }

    std::size_t offset = 0;
    while (connection.buffer.size() - offset >= Net::WireHeaderSize) {
        Net::Control::eCommand command;
        std::uint16_t          payloadLength = 0;
        if (!Net::Control::readHeader(connection.buffer.data() + offset,
                                      connection.buffer.size() - offset, command, payloadLength)) {
            closeConnection(connection);
            return;
        }
        const auto frameSize = Net::WireHeaderSize + payloadLength;
        if (connection.buffer.size() - offset < frameSize)
            break;
        handleCommand(connection, command, connection.buffer.data() + offset + Net::WireHeaderSize,
                      payloadLength);
        if (!connection.socket.is_open())
            return;
        offset += frameSize;
    }
    if (offset > 0)
        connection.buffer.erase(connection.buffer.begin(),
                                connection.buffer.begin() + static_cast<std::ptrdiff_t>(offset));
}

void CMasterServer::handleCommand(SConnection& connection, Net::Control::eCommand command,
                                  const std::uint8_t* payload, std::size_t payloadSize) {
    switch (command) {
        case Net::Control::eCommand::REGISTER: {
            Net::Control::SRegister registration;
            if (!Net::Control::readRegister(payload, payloadSize, registration)) {
                sendFrame(connection,
                          Net::Control::makeRegisterAck({
                              .result  = Net::Control::eRegisterResult::INVALID,
                              .message = "Malformed registration",
                          }));
                closeConnection(connection);
                return;
            }
            std::string message;
            const auto  result = mRegistry.registerServer(connection.id, registration, message);
            sendFrame(connection, Net::Control::makeRegisterAck({.result = result, .message = message}));
            if (result != Net::Control::eRegisterResult::ACCEPTED) {
                closeConnection(connection);
                return;
            }
            connection.registered = true;
            std::cout << "[Master] Registered '" << registration.name << "' at " << registration.address
                      << ':' << registration.tcpPort << " (udp " << registration.udpPort << ")\n";
            return;
        }
        case Net::Control::eCommand::STATUS: {
            Net::Control::SStatus status;
            if (!Net::Control::readStatus(payload, payloadSize, status)) {
                std::cerr << "[Master] Malformed status from connection " << connection.id << '\n';
                return;
            }
            std::string message;
            if (!mRegistry.updateStatus(connection.id, status, message))
                std::cerr << "[Master] Rejected status from connection " << connection.id << ": "
                          << message << '\n';
            return;
        }
        case Net::Control::eCommand::UNREGISTER: {
            if (!Net::Control::readUnregister(payload, payloadSize)) {
                sendFrame(connection,
                          Net::Control::makeError(Net::Control::eErrorCode::MALFORMED,
                                                  "Malformed unregister"));
                return;
            }
            mRegistry.unregisterServer(connection.id);
            sendFrame(connection,
                      Net::Control::makeUnregisterAck({.result = 0, .message = "Unregistered"}));
            closeConnection(connection);
            return;
        }
        case Net::Control::eCommand::SERVER_LIST_REQUEST: {
            std::uint8_t major = 0;
            std::uint8_t minor = 0;
            if (!Net::Control::readServerListRequest(payload, payloadSize, major, minor)) {
                sendFrame(connection,
                          Net::Control::makeError(Net::Control::eErrorCode::MALFORMED,
                                                  "Malformed server list request"));
                return;
            }
            Net::Control::SServerList list;
            list.entries = mRegistry.listServers(major, minor);
            sendFrame(connection, Net::Control::makeServerList(list));
            return;
        }
        default:
            sendFrame(connection,
                      Net::Control::makeError(Net::Control::eErrorCode::UNSUPPORTED,
                                              "Unsupported control command"));
            closeConnection(connection);
            return;
    }
}

void CMasterServer::sendFrame(SConnection& connection, const std::vector<std::uint8_t>& frame) {
    if (frame.empty() || !connection.socket.is_open())
        return;
    asio::error_code blockingError;
    if (connection.socket.non_blocking(false, blockingError)) {
        std::cerr << "[Master] Failed to set blocking mode: " << blockingError.message() << '\n';
    }
    asio::error_code writeError;
    asio::write(connection.socket, asio::buffer(frame), writeError);
    asio::error_code nonBlockingError;
    if (connection.socket.non_blocking(true, nonBlockingError)) {
        std::cerr << "[Master] Failed to restore non-blocking mode: " << nonBlockingError.message()
                  << '\n';
    }
    if (writeError)
        std::cerr << "[Master] Failed to send frame: " << writeError.message() << '\n';
}

void CMasterServer::closeConnection(SConnection& connection) {
    if (connection.closed)
        return;
    connection.closed = true;
    mRegistry.removeConnection(connection.id);
    asio::error_code error;
    if (connection.socket.is_open() && connection.socket.close(error)) {
        std::cerr << "[Master] Failed to close connection socket: " << error.message() << '\n';
    }
    std::cout << "[Master] Connection " << connection.id << " closed\n";
}

void CMasterServer::healthCheck() {
    const auto now = std::chrono::steady_clock::now();
    for (auto connection = mConnections.begin(); connection != mConnections.end();) {
        if (!connection->registered) {
            ++connection;
            continue;
        }
        if (!mRegistry.checkHealth(connection->id, now, ResponseDeadline, UnhealthyTimeout)) {
            closeConnection(*connection);
            ++connection;
            continue;
        }
        const auto sequence = mRegistry.beginHealthCheck(connection->id);
        if (sequence)
            sendFrame(*connection, Net::Control::makeKeepAlive(*sequence));
        ++connection;
    }
}
