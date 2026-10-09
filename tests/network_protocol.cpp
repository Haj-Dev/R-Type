#include "Shared/Types.hpp"
#include "Shared/Components/Velocity.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <type_traits>
#include <vector>

namespace {

    static_assert(std::is_base_of_v<AComponent, SCPosition>);
    static_assert(std::is_base_of_v<AComponent, SCVelocity>);
    static_assert(std::is_base_of_v<AComponent, SCDrawable>);

    template <typename T>
    T readBytes(const std::vector<std::uint8_t>& packet, std::size_t offset) {
        T value{};
        std::memcpy(&value, packet.data() + offset, sizeof(value));
        return value;
    }

} // namespace

TEST(NetworkProtocol, TcpHelloRoundTripsUdpPort) {
    const auto    packet = Net::makeTcpHello(41234);
    std::uint16_t port   = 0;

    ASSERT_TRUE(Net::readTcpHello(packet.data(), packet.size(), port));
    EXPECT_EQ(port, 41234);
}

TEST(NetworkProtocol, TcpHelloRejectsInvalidPackets) {
    const auto    valid = Net::makeTcpHello(7001);
    std::uint16_t port  = 0;

    auto          wrongMagic = valid;
    wrongMagic.at(0)         = 'X';
    EXPECT_FALSE(Net::readTcpHello(wrongMagic.data(), wrongMagic.size(), port));

    auto wrongTerminator   = valid;
    wrongTerminator.back() = 0;
    EXPECT_FALSE(Net::readTcpHello(wrongTerminator.data(), wrongTerminator.size(), port));

    auto zeroPort = Net::makeTcpHello(0);
    EXPECT_FALSE(Net::readTcpHello(zeroPort.data(), zeroPort.size(), port));

    EXPECT_FALSE(Net::readTcpHello(valid.data(), valid.size() - 1, port));
}

TEST(NetworkProtocol, TcpWelcomeRoundTripsPlayerId) {
    const auto   packet   = Net::makeTcpWelcome(3);
    std::uint8_t playerId = 0;

    ASSERT_TRUE(Net::readTcpWelcome(packet.data(), packet.size(), playerId));
    EXPECT_EQ(playerId, 3);
}

TEST(NetworkProtocol, TcpWelcomeRejectsMalformedPackets) {
    const auto   valid    = Net::makeTcpWelcome(1);
    std::uint8_t playerId = 0;

    auto         wrongMagic = valid;
    wrongMagic.at(0)        = 'X';
    EXPECT_FALSE(Net::readTcpWelcome(wrongMagic.data(), wrongMagic.size(), playerId));

    EXPECT_FALSE(Net::readTcpWelcome(valid.data(), valid.size() - 1, playerId));
}

TEST(NetworkProtocol, TcpWelcomeRejectsOutOfRangePlayerId) {
    const auto   packet   = Net::makeTcpWelcome(static_cast<std::uint8_t>(SPLayerActions::MaxPlayers));
    std::uint8_t playerId = 0;

    EXPECT_FALSE(Net::readTcpWelcome(packet.data(), packet.size(), playerId));
}

TEST(NetworkProtocol, InputRoundTripsEveryActionFlag) {
    const SPlayerActionState expected{
        .up    = true,
        .down  = true,
        .left  = true,
        .right = true,
        .fire  = true,
    };
    const auto         packet = Net::makeInput(expected);
    SPlayerActionState actual;

    ASSERT_TRUE(Net::readInput(packet.data(), packet.size(), actual));
    EXPECT_EQ(actual.up, expected.up);
    EXPECT_EQ(actual.down, expected.down);
    EXPECT_EQ(actual.left, expected.left);
    EXPECT_EQ(actual.right, expected.right);
    EXPECT_EQ(actual.fire, expected.fire);
}

TEST(NetworkProtocol, InputRejectsWrongTypeAndSize) {
    const SPlayerActionState action{.right = true};
    const auto               valid = Net::makeInput(action);
    SPlayerActionState       decoded;

    auto                     wrongMagic = valid;
    wrongMagic.at(1)                    = 'X';
    EXPECT_FALSE(Net::readInput(wrongMagic.data(), wrongMagic.size(), decoded));

    auto wrongCommand                = valid;
    wrongCommand.at(Net::HeaderSize) = static_cast<std::uint8_t>(Net::eCommandType::HELLO);
    EXPECT_FALSE(Net::readInput(wrongCommand.data(), wrongCommand.size(), decoded));

    EXPECT_FALSE(Net::readInput(valid.data(), valid.size() - 1, decoded));
}

TEST(NetworkProtocol, PacketValidationChecksExpectedTypeAndTerminator) {
    const auto hello = Net::makeHello();

    EXPECT_TRUE(Net::validPacket(hello.data(), hello.size(), Net::ePacketType::HELLO));
    EXPECT_FALSE(Net::validPacket(hello.data(), hello.size(), Net::ePacketType::INPUT));

    auto wrongMagic  = hello;
    wrongMagic.at(0) = 'X';
    EXPECT_FALSE(Net::validPacket(wrongMagic.data(), wrongMagic.size(), Net::ePacketType::HELLO));

    auto wrongFirstTerminator                                = hello;
    wrongFirstTerminator.at(wrongFirstTerminator.size() - 2) = 0;
    EXPECT_FALSE(Net::validPacket(wrongFirstTerminator.data(), wrongFirstTerminator.size(),
                                  Net::ePacketType::HELLO));

    auto wrongSecondTerminator   = hello;
    wrongSecondTerminator.back() = 0;
    EXPECT_FALSE(Net::validPacket(wrongSecondTerminator.data(), wrongSecondTerminator.size(),
                                  Net::ePacketType::HELLO));

    auto missingTerminator = hello;
    missingTerminator.pop_back();
    EXPECT_FALSE(
        Net::validPacket(missingTerminator.data(), missingTerminator.size(), Net::ePacketType::HELLO));
}

TEST(NetworkProtocol, WelcomePacketRoundTripsPlayerId) {
    const auto   packet   = Net::makeWelcome(2);
    std::uint8_t playerId = 0;

    ASSERT_TRUE(Net::readWelcome(packet.data(), packet.size(), playerId));
    EXPECT_EQ(playerId, 2);
}

TEST(NetworkProtocol, WelcomePacketRejectsMalformedCommandAndSize) {
    const auto   valid    = Net::makeWelcome(1);
    std::uint8_t playerId = 0;

    auto         wrongCommand        = valid;
    wrongCommand.at(Net::HeaderSize) = static_cast<std::uint8_t>(Net::eCommandType::HELLO);
    EXPECT_FALSE(Net::readWelcome(wrongCommand.data(), wrongCommand.size(), playerId));

    EXPECT_FALSE(Net::readWelcome(valid.data(), valid.size() - 1, playerId));

    auto invalidPlayer = Net::makeWelcome(static_cast<std::uint8_t>(SPLayerActions::MaxPlayers));
    EXPECT_FALSE(Net::readWelcome(invalidPlayer.data(), invalidPlayer.size(), playerId));
}

TEST(NetworkProtocol, SnapshotEncodesEntityFields) {
    SSceneData scene;
    scene.entities.push_back(Game::SSnapshotEntity{
        .id     = 42,
        .kind   = Game::eEntityKind::PLAYER,
        .x      = 123.5F,
        .y      = 456.25F,
        .health = 3,
    });

    const auto            packet        = Net::makeSnapshot(scene);
    constexpr std::size_t commandOffset = Net::HeaderSize;
    constexpr std::size_t payloadOffset = commandOffset + Net::CommandHeaderSize;

    ASSERT_TRUE(Net::validPacket(packet.data(), packet.size(), Net::ePacketType::SNAPSHOT));
    ASSERT_EQ(packet.at(commandOffset), static_cast<std::uint8_t>(Net::eCommandType::SNAPSHOT_ENTITY));
    EXPECT_EQ(readBytes<std::uint64_t>(packet, commandOffset + 1), 42U);
    EXPECT_EQ(static_cast<Game::eEntityKind>(packet.at(payloadOffset)), Game::eEntityKind::PLAYER);
    EXPECT_FLOAT_EQ(readBytes<float>(packet, payloadOffset + 1), 123.5F);
    EXPECT_FLOAT_EQ(readBytes<float>(packet, payloadOffset + 5), 456.25F);
    EXPECT_EQ(readBytes<std::int16_t>(packet, payloadOffset + 9), 3);
}

TEST(SceneData, SnapshotProjectionBuildsComponentRegistry) {
    SSceneData scene;
    scene.replaceEntities({
        Game::SSnapshotEntity{
            .id     = 7,
            .kind   = Game::eEntityKind::ENEMY,
            .x      = 12.0F,
            .y      = 34.0F,
            .health = 2,
        },
    });

    ASSERT_EQ(scene.renderRegistry.size(), 1U);
    scene.renderRegistry.each<SCPosition, SCEntityType, SCHealth>(
        [&](Ecs::SEntity, SCPosition& position, SCEntityType& type, SCHealth& health) {
            EXPECT_FLOAT_EQ(position.x, 12.0F);
            EXPECT_FLOAT_EQ(position.y, 34.0F);
            EXPECT_EQ(type.value, Game::eEntityKind::ENEMY);
            EXPECT_EQ(health.value, 2);
        });
    EXPECT_EQ(scene.renderRegistry.pool<SCDrawable>().size(), 1U);
}

TEST(SceneData, SnapshotProjectionKeepsProjectileWithoutHealthComponent) {
    SSceneData scene;
    scene.replaceEntities({
        Game::SSnapshotEntity{
            .id   = 8,
            .kind = Game::eEntityKind::PLAYER_PROJECTILE,
            .x    = 42.0F,
            .y    = 24.0F,
        },
    });

    ASSERT_EQ(scene.renderRegistry.size(), 1U);
    ASSERT_TRUE(scene.renderRegistry.has<SCDrawable>(*scene.renderRegistry.view<SCPosition>().begin()));
    scene.renderRegistry.each<SCPosition, SCEntityType>(
        [&](Ecs::SEntity entity, SCPosition& position, SCEntityType& type) {
            EXPECT_FLOAT_EQ(position.x, 42.0F);
            EXPECT_FLOAT_EQ(position.y, 24.0F);
            EXPECT_EQ(type.value, Game::eEntityKind::PLAYER_PROJECTILE);
            EXPECT_EQ(scene.renderRegistry.get<SCHealth>(entity), nullptr);
        });
}
