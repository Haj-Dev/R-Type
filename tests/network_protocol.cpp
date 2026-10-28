#include "Shared/Types.hpp"
#include "Shared/Components/Velocity.hpp"

#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

namespace {

    static_assert(std::is_base_of_v<AComponent, SCPosition>);
    static_assert(std::is_base_of_v<AComponent, SCVelocity>);
    static_assert(std::is_base_of_v<AComponent, SCDrawableShape>);

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

    auto wrongSecondTerminator                                 = hello;
    wrongSecondTerminator.at(wrongSecondTerminator.size() - 1) = 0;
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
    SSceneData            scene;
    Game::SSnapshotEntity entity{
        .id      = 42,
        .kind    = Game::eEntityKind::PLAYER,
        .x       = 123.5F,
        .y       = 456.25F,
        .health  = 3,
        .texture = "assets/player.png",
    };
    entity.atlas         = true;
    entity.baseDimension = SDimentions{.width = 32U, .height = 24U};
    entity.frames        = 6U;
    entity.frameDuration = 0.12F;
    entity.variant       = 2U;
    scene.entities.push_back(entity);

    scene.tick = 123;
    const auto packet = Net::makeSnapshot(scene);
    ASSERT_TRUE(Net::validPacket(packet.data(), packet.size(), Net::ePacketType::SNAPSHOT));
    std::vector<Game::SSnapshotEntity> decoded;
    std::uint64_t tick = 0;
    ASSERT_TRUE(Net::readSnapshot(packet.data(), packet.size(), decoded, tick));
    EXPECT_EQ(tick, 123U);
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().id, 42U);
    EXPECT_EQ(decoded.front().kind, Game::eEntityKind::PLAYER);
    EXPECT_FLOAT_EQ(decoded.front().x, 123.5F);
    EXPECT_FLOAT_EQ(decoded.front().y, 456.25F);
    EXPECT_EQ(decoded.front().health, 3);
    EXPECT_EQ(decoded.front().texture, "assets/player.png");
    EXPECT_TRUE(decoded.front().atlas);
    EXPECT_EQ(decoded.front().baseDimension.width, 32U);
    EXPECT_EQ(decoded.front().baseDimension.height, 24U);
    EXPECT_EQ(decoded.front().frames, 6U);
    EXPECT_FLOAT_EQ(decoded.front().frameDuration, 0.12F);
    EXPECT_EQ(decoded.front().variant, 2U);
}

TEST(NetworkProtocol, DrawableLayerUsesOneByteWireValue) {
    const SCDrawableShape     original{eDrawableShape::CIRCLE, 8.0F, 1, 127};
    std::vector<std::uint8_t> bytes;
    original.serialize(bytes);

    ASSERT_EQ(bytes.size(),
              sizeof(AComponent::eType) + sizeof(eDrawableShape) + sizeof(float) + sizeof(std::uint8_t) +
                  sizeof(std::int8_t));
    CComponentRegistry registry;
    const auto         component = registry.deserialize(bytes);
    const auto*        decoded   = dynamic_cast<const SCDrawableShape*>(component.get());
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->layer, 127);
}

TEST(NetworkProtocol, SnapshotRejectsTruncatedComponentPayload) {
    SSceneData scene;
    scene.entities.push_back(Game::SSnapshotEntity{.id = 42, .texture = "sprite.png"});
    auto packet = Net::makeSnapshot(scene);
    packet.erase(packet.end() - 3);
    std::vector<Game::SSnapshotEntity> decoded;

    EXPECT_FALSE(Net::readSnapshot(packet.data(), packet.size(), decoded));
}

TEST(NetworkProtocol, SnapshotRoundTripsBeyondLegacyReceiveBuffers) {
    SSceneData scene;
    scene.entities.push_back(Game::SSnapshotEntity{
        .id      = 42,
        .texture = std::string(5000, 'x'),
    });

    const auto packet = Net::makeSnapshot(scene);
    ASSERT_GT(packet.size(), 4096U);
    ASSERT_LT(packet.size(), Net::MaxUdpPayloadSize);

    std::vector<Game::SSnapshotEntity> decoded;
    ASSERT_TRUE(Net::readSnapshot(packet.data(), packet.size(), decoded));
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().texture, scene.entities.front().texture);
}

TEST(NetworkProtocol, UpdateRoundTripsPartialComponentsAndRemoval) {
    std::vector<std::uint8_t> position;
    SCPosition{12.5F, 42.0F}.serialize(position);
    const std::vector<Game::SUpdateEntity> updates{
        Game::SUpdateEntity{.id = 7, .components = {position}},
        Game::SUpdateEntity{.id = 9, .remove = true},
    };

    const auto packet = Net::makeUpdate(456, updates);
    ASSERT_TRUE(Net::validPacket(packet.data(), packet.size(), Net::ePacketType::UPDATE));
    std::vector<Game::SUpdateEntity> decoded;
    std::uint64_t tick = 0;
    ASSERT_TRUE(Net::readUpdate(packet.data(), packet.size(), decoded, tick));
    EXPECT_EQ(tick, 456U);
    ASSERT_EQ(decoded.size(), 2U);
    ASSERT_EQ(decoded.at(0).components.size(), 1U);
    EXPECT_EQ(decoded.at(0).id, 7U);
    EXPECT_EQ(decoded.at(0).components.front(), position);
    EXPECT_EQ(decoded.at(1).id, 9U);
    EXPECT_TRUE(decoded.at(1).remove);
}

TEST(NetworkProtocol, UpdateRejectsTruncatedComponentPayload) {
    std::vector<std::uint8_t> position;
    SCPosition{12.5F, 42.0F}.serialize(position);
    auto packet = Net::makeUpdate({Game::SUpdateEntity{.id = 7, .components = {position}}});
    packet.erase(packet.end() - 3);

    std::vector<Game::SUpdateEntity> decoded;
    EXPECT_FALSE(Net::readUpdate(packet.data(), packet.size(), decoded));
}

TEST(ComponentSerialization, RegistryRoundTripsPosition) {
    const SCPosition          original{12.5F, -4.0F};
    std::vector<std::uint8_t> bytes;
    original.serialize(bytes);

    CComponentRegistry registry;
    const auto         component = registry.deserialize(bytes);
    const auto*        position  = dynamic_cast<const SCPosition*>(component.get());
    ASSERT_NE(position, nullptr);
    EXPECT_FLOAT_EQ(position->x, original.x);
    EXPECT_FLOAT_EQ(position->y, original.y);
}

TEST(SceneData, SnapshotProjectionBuildsComponentRegistry) {
    SSceneData scene;
    scene.replaceEntities({
        Game::SSnapshotEntity{
            .id      = 7,
            .kind    = Game::eEntityKind::ENEMY,
            .x       = 12.0F,
            .y       = 34.0F,
            .health  = 2,
            .texture = {},
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
    EXPECT_EQ(scene.renderRegistry.pool<SCDrawableShape>().size(), 1U);
}

TEST(SceneData, SnapshotProjectionKeepsProjectileWithoutHealthComponent) {
    SSceneData scene;
    scene.replaceEntities({
        Game::SSnapshotEntity{
            .id      = 8,
            .kind    = Game::eEntityKind::PLAYER_PROJECTILE,
            .x       = 42.0F,
            .y       = 24.0F,
            .texture = {},
        },
    });

    ASSERT_EQ(scene.renderRegistry.size(), 1U);
    ASSERT_TRUE(
        scene.renderRegistry.has<SCDrawableShape>(*scene.renderRegistry.view<SCPosition>().begin()));
    scene.renderRegistry.each<SCPosition, SCEntityType>(
        [&](Ecs::SEntity entity, SCPosition& position, SCEntityType& type) {
            EXPECT_FLOAT_EQ(position.x, 42.0F);
            EXPECT_FLOAT_EQ(position.y, 24.0F);
            EXPECT_EQ(type.value, Game::eEntityKind::PLAYER_PROJECTILE);
            EXPECT_EQ(scene.renderRegistry.get<SCHealth>(entity), nullptr);
        });
}

TEST(SceneData, UpdatesMergeComponentsAndRemoveEntities) {
    SSceneData scene;
    scene.replaceEntities({
        Game::SSnapshotEntity{.id = 7, .x = 1.0F, .y = 2.0F, .health = 5},
        Game::SSnapshotEntity{.id = 9, .x = 3.0F},
    });
    std::vector<std::uint8_t> position;
    SCPosition{12.5F, 42.0F}.serialize(position);
    std::vector<std::uint8_t> health;
    SCHealth{2}.serialize(health);

    ASSERT_TRUE(scene.applyUpdates({
        Game::SUpdateEntity{.id = 7, .components = {position, health}},
        Game::SUpdateEntity{.id = 9, .remove = true},
    }));
    ASSERT_EQ(scene.entities.size(), 1U);
    EXPECT_EQ(scene.entities.front().id, 7U);
    EXPECT_FLOAT_EQ(scene.entities.front().x, 12.5F);
    EXPECT_FLOAT_EQ(scene.entities.front().y, 42.0F);
    EXPECT_EQ(scene.entities.front().health, 2);
}
