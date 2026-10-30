#include "Master/MasterRegistry.hpp"
#include "Shared/ControlProtocol.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace {

    Net::Control::SRegister makeRegistration(const std::string& name = "server1") {
        return Net::Control::SRegister{
            .name       = name,
            .tcpPort    = 7000,
            .udpPort    = 7001,
            .maxPlayers = 4,
            .major      = Net::Control::ProtocolMajor,
            .minor      = Net::Control::ProtocolMinor,
            .address    = "127.0.0.1",
        };
    }

} // namespace

TEST(ControlProtocol, RegisterRoundTrips) {
    const auto registration = makeRegistration();
    const auto frame        = Net::Control::makeRegister(registration);
    ASSERT_FALSE(frame.empty());

    Net::Control::eCommand command;
    std::uint16_t          payloadLength = 0;
    ASSERT_TRUE(Net::Control::readHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::Control::eCommand::REGISTER);

    Net::Control::SRegister decoded;
    ASSERT_TRUE(Net::Control::readRegister(frame.data() + Net::WireHeaderSize, payloadLength, decoded));
    EXPECT_EQ(decoded.name, "server1");
    EXPECT_EQ(decoded.tcpPort, 7000);
    EXPECT_EQ(decoded.udpPort, 7001);
    EXPECT_EQ(decoded.maxPlayers, 4);
    EXPECT_EQ(decoded.major, Net::Control::ProtocolMajor);
    EXPECT_EQ(decoded.minor, Net::Control::ProtocolMinor);
    EXPECT_EQ(decoded.address, "127.0.0.1");
}

TEST(ControlProtocol, RegisterRejectsInvalidFields) {
    auto registration    = makeRegistration();
    registration.tcpPort = 0;
    EXPECT_TRUE(Net::Control::makeRegister(registration).empty());

    registration = makeRegistration();
    registration.address.clear();
    EXPECT_TRUE(Net::Control::makeRegister(registration).empty());

    registration = makeRegistration("toolongname");
    EXPECT_TRUE(Net::Control::makeRegister(registration).empty());
}

TEST(ControlProtocol, RegisterAckRoundTrips) {
    const auto frame = Net::Control::makeRegisterAck(
        {.result = Net::Control::eRegisterResult::CONFLICT, .message = "Name taken"});

    Net::Control::eCommand command;
    std::uint16_t          payloadLength = 0;
    ASSERT_TRUE(Net::Control::readHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::Control::eCommand::REGISTER_ACK);

    Net::Control::SRegisterAck ack;
    ASSERT_TRUE(Net::Control::readRegisterAck(frame.data() + Net::WireHeaderSize, payloadLength, ack));
    EXPECT_EQ(ack.result, Net::Control::eRegisterResult::CONFLICT);
    EXPECT_EQ(ack.message, "Name taken");
}

TEST(ControlProtocol, KeepAliveAndStatusRoundTrip) {
    const auto             keepAlive = Net::Control::makeKeepAlive(42);
    Net::Control::eCommand command;
    std::uint16_t          payloadLength = 0;
    ASSERT_TRUE(Net::Control::readHeader(keepAlive.data(), keepAlive.size(), command, payloadLength));
    EXPECT_EQ(command, Net::Control::eCommand::KEEP_ALIVE);
    std::uint8_t sequence = 0;
    ASSERT_TRUE(
        Net::Control::readKeepAlive(keepAlive.data() + Net::WireHeaderSize, payloadLength, sequence));
    EXPECT_EQ(sequence, 42);

    const Net::Control::SStatus status{
        .rooms            = 2,
        .connectedPlayers = 3,
        .maxPlayers       = 4,
        .joinableRooms    = 1,
        .sequence         = 42,
        .state            = Net::Control::eServerState::READY,
    };
    const auto statusFrame = Net::Control::makeStatus(status);
    ASSERT_TRUE(
        Net::Control::readHeader(statusFrame.data(), statusFrame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::Control::eCommand::STATUS);
    Net::Control::SStatus decoded;
    ASSERT_TRUE(
        Net::Control::readStatus(statusFrame.data() + Net::WireHeaderSize, payloadLength, decoded));
    EXPECT_EQ(decoded.rooms, 2);
    EXPECT_EQ(decoded.connectedPlayers, 3);
    EXPECT_EQ(decoded.maxPlayers, 4);
    EXPECT_EQ(decoded.joinableRooms, 1);
    EXPECT_EQ(decoded.sequence, 42);
    EXPECT_EQ(decoded.state, Net::Control::eServerState::READY);
}

TEST(ControlProtocol, ServerListRoundTrips) {
    Net::Control::SServerList list;
    list.entries.push_back(Net::Control::SServerEntry{
        .name             = "server1",
        .tcpPort          = 7000,
        .udpPort          = 7001,
        .connectedPlayers = 2,
        .maxPlayers       = 4,
        .rooms            = 1,
        .joinableRooms    = 1,
        .major            = Net::Control::ProtocolMajor,
        .minor            = Net::Control::ProtocolMinor,
        .state            = Net::Control::eServerState::READY,
        .address          = "127.0.0.1",
    });
    list.entries.push_back(Net::Control::SServerEntry{
        .name             = "server2",
        .tcpPort          = 7010,
        .udpPort          = 7011,
        .connectedPlayers = 0,
        .maxPlayers       = 4,
        .rooms            = 0,
        .joinableRooms    = 0,
        .major            = Net::Control::ProtocolMajor,
        .minor            = Net::Control::ProtocolMinor,
        .state            = Net::Control::eServerState::READY,
        .address          = "10.0.0.2",
    });

    const auto             frame = Net::Control::makeServerList(list);
    Net::Control::eCommand command;
    std::uint16_t          payloadLength = 0;
    ASSERT_TRUE(Net::Control::readHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::Control::eCommand::SERVER_LIST);

    Net::Control::SServerList decoded;
    ASSERT_TRUE(
        Net::Control::readServerList(frame.data() + Net::WireHeaderSize, payloadLength, decoded));
    ASSERT_EQ(decoded.entries.size(), 2U);
    EXPECT_EQ(decoded.entries.at(0).name, "server1");
    EXPECT_EQ(decoded.entries.at(0).tcpPort, 7000);
    EXPECT_EQ(decoded.entries.at(0).connectedPlayers, 2);
    EXPECT_EQ(decoded.entries.at(0).address, "127.0.0.1");
    EXPECT_EQ(decoded.entries.at(1).name, "server2");
    EXPECT_EQ(decoded.entries.at(1).address, "10.0.0.2");
}

TEST(ControlProtocol, HeaderRejectsBadMagicAndOversizedPayload) {
    const auto frame =
        Net::Control::makeServerListRequest(Net::Control::ProtocolMajor, Net::Control::ProtocolMinor);
    Net::Control::eCommand command;
    std::uint16_t          payloadLength = 0;
    ASSERT_TRUE(Net::Control::readHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::Control::eCommand::SERVER_LIST_REQUEST);

    auto badMagic  = frame;
    badMagic.at(0) = 'X';
    EXPECT_FALSE(Net::Control::readHeader(badMagic.data(), badMagic.size(), command, payloadLength));

    auto oversized  = frame;
    oversized.at(4) = 0xFF;
    oversized.at(5) = 0xFF;
    EXPECT_FALSE(Net::Control::readHeader(oversized.data(), oversized.size(), command, payloadLength));
}

TEST(MasterRegistry, RegistersAndListsReadyServers) {
    Master::CMasterRegistry registry;
    std::string             message;
    EXPECT_EQ(registry.registerServer(1, makeRegistration(), message),
              Net::Control::eRegisterResult::ACCEPTED);
    EXPECT_EQ(registry.size(), 1U);
    EXPECT_TRUE(registry.listServers(Net::Control::ProtocolMajor, Net::Control::ProtocolMinor).empty());

    const auto sequence = registry.beginHealthCheck(1);
    ASSERT_TRUE(sequence.has_value());
    const Net::Control::SStatus status{
        .rooms            = 0,
        .connectedPlayers = 0,
        .maxPlayers       = 4,
        .joinableRooms    = 0,
        .sequence         = *sequence,
        .state            = Net::Control::eServerState::READY,
    };
    ASSERT_TRUE(registry.updateStatus(1, status, message));

    const auto servers = registry.listServers(Net::Control::ProtocolMajor, Net::Control::ProtocolMinor);
    ASSERT_EQ(servers.size(), 1U);
    EXPECT_EQ(servers.at(0).name, "server1");
    EXPECT_EQ(servers.at(0).state, Net::Control::eServerState::READY);
}

TEST(MasterRegistry, DuplicateNameFromAnotherConnectionConflicts) {
    Master::CMasterRegistry registry;
    std::string             message;
    ASSERT_EQ(registry.registerServer(1, makeRegistration(), message),
              Net::Control::eRegisterResult::ACCEPTED);
    EXPECT_EQ(registry.registerServer(2, makeRegistration(), message),
              Net::Control::eRegisterResult::CONFLICT);
    EXPECT_EQ(registry.size(), 1U);
}

TEST(MasterRegistry, ReRegisterSameConnectionUpdatesEntry) {
    Master::CMasterRegistry registry;
    std::string             message;
    ASSERT_EQ(registry.registerServer(1, makeRegistration(), message),
              Net::Control::eRegisterResult::ACCEPTED);

    auto updated       = makeRegistration();
    updated.tcpPort    = 7100;
    updated.udpPort    = 7101;
    updated.maxPlayers = 8;
    EXPECT_EQ(registry.registerServer(1, updated, message), Net::Control::eRegisterResult::ACCEPTED);
    EXPECT_EQ(registry.size(), 1U);

    const auto registration = registry.registration(1);
    ASSERT_TRUE(registration.has_value());
    EXPECT_EQ(registration->tcpPort, 7100);
    EXPECT_EQ(registration->udpPort, 7101);
    EXPECT_EQ(registration->maxPlayers, 8);
}

TEST(MasterRegistry, RenamingActiveRegistrationIsRejected) {
    Master::CMasterRegistry registry;
    std::string             message;
    ASSERT_EQ(registry.registerServer(1, makeRegistration("server1"), message),
              Net::Control::eRegisterResult::ACCEPTED);
    EXPECT_EQ(registry.registerServer(1, makeRegistration("server2"), message),
              Net::Control::eRegisterResult::INVALID);
}

TEST(MasterRegistry, InvalidRegistrationIsRejected) {
    Master::CMasterRegistry registry;
    std::string             message;

    auto                    invalidAddress = makeRegistration();
    invalidAddress.address                 = "0.0.0.0";
    EXPECT_EQ(registry.registerServer(1, invalidAddress, message),
              Net::Control::eRegisterResult::INVALID);

    auto incompatible  = makeRegistration();
    incompatible.major = Net::Control::ProtocolMajor + 1;
    EXPECT_EQ(registry.registerServer(2, incompatible, message),
              Net::Control::eRegisterResult::INCOMPATIBLE);
}

TEST(MasterRegistry, StatusRequiresMatchingOutstandingProbe) {
    Master::CMasterRegistry registry;
    std::string             message;
    ASSERT_EQ(registry.registerServer(1, makeRegistration(), message),
              Net::Control::eRegisterResult::ACCEPTED);

    Net::Control::SStatus status{
        .rooms            = 1,
        .connectedPlayers = 1,
        .maxPlayers       = 4,
        .joinableRooms    = 1,
        .sequence         = 0,
        .state            = Net::Control::eServerState::READY,
    };
    EXPECT_FALSE(registry.updateStatus(1, status, message));

    const auto sequence = registry.beginHealthCheck(1);
    ASSERT_TRUE(sequence.has_value());
    status.sequence = *sequence;
    EXPECT_TRUE(registry.updateStatus(1, status, message));

    const auto servers = registry.listServers(Net::Control::ProtocolMajor, Net::Control::ProtocolMinor);
    ASSERT_EQ(servers.size(), 1U);
    EXPECT_EQ(servers.at(0).state, Net::Control::eServerState::READY);
    EXPECT_EQ(servers.at(0).connectedPlayers, 1);
}

TEST(MasterRegistry, StatusSequenceMismatchIsRejected) {
    Master::CMasterRegistry registry;
    std::string             message;
    ASSERT_EQ(registry.registerServer(1, makeRegistration(), message),
              Net::Control::eRegisterResult::ACCEPTED);
    const auto sequence = registry.beginHealthCheck(1);
    ASSERT_TRUE(sequence.has_value());

    Net::Control::SStatus status{
        .rooms            = 0,
        .connectedPlayers = 0,
        .maxPlayers       = 4,
        .joinableRooms    = 0,
        .sequence         = static_cast<std::uint8_t>(*sequence + 1),
        .state            = Net::Control::eServerState::READY,
    };
    EXPECT_FALSE(registry.updateStatus(1, status, message));
}

TEST(MasterRegistry, StatusValidationRejectsInconsistentCounts) {
    Master::CMasterRegistry registry;
    std::string             message;
    ASSERT_EQ(registry.registerServer(1, makeRegistration(), message),
              Net::Control::eRegisterResult::ACCEPTED);
    const auto sequence = registry.beginHealthCheck(1);
    ASSERT_TRUE(sequence.has_value());

    Net::Control::SStatus status{
        .rooms            = 1,
        .connectedPlayers = 5,
        .maxPlayers       = 4,
        .joinableRooms    = 1,
        .sequence         = *sequence,
        .state            = Net::Control::eServerState::READY,
    };
    EXPECT_FALSE(registry.updateStatus(1, status, message));

    status.connectedPlayers = 1;
    status.joinableRooms    = 2;
    EXPECT_FALSE(registry.updateStatus(1, status, message));

    status.joinableRooms = 1;
    status.maxPlayers    = 8;
    EXPECT_FALSE(registry.updateStatus(1, status, message));
}

TEST(MasterRegistry, HealthTimeoutClosesAndUnhealthyServersAreExcluded) {
    Master::CMasterRegistry registry;
    std::string             message;
    ASSERT_EQ(registry.registerServer(1, makeRegistration(), message),
              Net::Control::eRegisterResult::ACCEPTED);

    const auto now = std::chrono::steady_clock::now();
    EXPECT_TRUE(registry.checkHealth(1, now + std::chrono::seconds(4), std::chrono::seconds(1),
                                     std::chrono::seconds(3)));
    EXPECT_TRUE(registry.listServers(Net::Control::ProtocolMajor, Net::Control::ProtocolMinor).empty());

    ASSERT_TRUE(registry.beginHealthCheck(1).has_value());
    EXPECT_FALSE(registry.checkHealth(1, now + std::chrono::seconds(6), std::chrono::seconds(1),
                                      std::chrono::seconds(3)));
}

TEST(MasterRegistry, UnregisterRemovesRegistration) {
    Master::CMasterRegistry registry;
    std::string             message;
    ASSERT_EQ(registry.registerServer(1, makeRegistration(), message),
              Net::Control::eRegisterResult::ACCEPTED);
    registry.unregisterServer(1);
    EXPECT_EQ(registry.size(), 0U);
    EXPECT_FALSE(registry.hasRegistration(1));
    EXPECT_TRUE(registry.listServers(Net::Control::ProtocolMajor, Net::Control::ProtocolMinor).empty());
}

TEST(MasterRegistry, IncompatibleClientVersionGetsNoServers) {
    Master::CMasterRegistry registry;
    std::string             message;
    ASSERT_EQ(registry.registerServer(1, makeRegistration(), message),
              Net::Control::eRegisterResult::ACCEPTED);
    EXPECT_TRUE(
        registry.listServers(Net::Control::ProtocolMajor + 1, Net::Control::ProtocolMinor).empty());
}
