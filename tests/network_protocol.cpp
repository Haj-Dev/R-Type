#include "Shared/Types.hpp"
#include "Server/NetworkThread/NetworkPolicy.hpp"

#include <algorithm>
#include <gtest/gtest.h>

#include <vector>

TEST(NetworkProtocol, TcpHelloRoundTripsUdpPort) {
    const auto    packet = Net::makeTcpHello(41234);
    std::uint16_t port   = 0;
    ASSERT_TRUE(Net::readTcpHello(packet.data(), packet.size(), port));
    EXPECT_EQ(port, 41234);
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

TEST(NetworkProtocol, TcpWelcomeAcceptsConnectionId) {
    const auto   packet   = Net::makeTcpWelcome(255);
    std::uint8_t playerId = 0;

    EXPECT_TRUE(Net::readTcpWelcome(packet.data(), packet.size(), playerId));
    EXPECT_EQ(playerId, 255);
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

TEST(NetworkProtocol, VoiceRoundTripsAudioFrame) {
    const std::vector<std::uint8_t> expected{0x01, 0x02, 0x03, 0x04};
    const auto                      packet   = Net::makeVoice(2, 17, expected);
    std::uint8_t                    playerId = 0;
    std::uint16_t                   sequence = 0;
    std::vector<std::uint8_t>       actual;
    ASSERT_TRUE(Net::readVoice(packet.data(), packet.size(), playerId, sequence, actual));
    EXPECT_EQ(playerId, 2);
    EXPECT_EQ(sequence, 17);
    EXPECT_EQ(actual, expected);
}

TEST(NetworkProtocol, VoiceRejectsTruncatedFrame) {
    const std::vector<std::uint8_t> payload{0x01, 0x02};
    auto                            packet = Net::makeVoice(1, 0, payload);
    packet.pop_back();
    std::uint8_t              playerId = 0;
    std::uint16_t             sequence = 0;
    std::vector<std::uint8_t> decoded;
    EXPECT_FALSE(Net::readVoice(packet.data(), packet.size(), playerId, sequence, decoded));
}

TEST(NetworkProtocol, SnapshotRoundTripsMultipleAndEmptyEntities) {
    SSceneData                scene;
    std::vector<std::uint8_t> position;
    SCPosition{123.5F, 456.25F}.serialize(position);
    scene.entities = {
        Game::SEntityComponents{.id = 2, .creationTick = 77, .components = {position}},
        Game::SEntityComponents{.id = 9, .creationTick = 88, .components = {}},
    };
    scene.tick = 123;

    const auto                           packet = Net::makeSnapshot(scene);
    std::vector<Game::SEntityComponents> decoded;
    std::uint64_t                        tick = 0;
    ASSERT_TRUE(Net::readSnapshot(packet.data(), packet.size(), decoded, tick));
    ASSERT_EQ(decoded.size(), 2U);
    EXPECT_EQ(tick, 123U);
    EXPECT_EQ(decoded.at(0).id, 2U);
    EXPECT_EQ(decoded.at(0).creationTick, 77U);
    ASSERT_EQ(decoded.at(0).components.size(), 1U);
    EXPECT_EQ(decoded.at(0).components.front(), position);
    EXPECT_EQ(decoded.at(1).id, 9U);
    EXPECT_EQ(decoded.at(1).creationTick, 88U);
    EXPECT_TRUE(decoded.at(1).components.empty());
}

TEST(NetworkProtocol, LevelStateIsIncludedInSnapshotsAndUpdates) {
    EXPECT_NE(std::ranges::find(SNAPSHOT_COMPONENTS, AComponent::eType::LEVEL_STATE),
              SNAPSHOT_COMPONENTS.end());
    EXPECT_TRUE(COMPONENT_UPDATE_POLICY.contains(AComponent::eType::LEVEL_STATE));
}

TEST(NetworkProtocol, SnapshotRejectsUnknownComponent) {
    SSceneData scene;
    scene.entities.push_back(Game::SEntityComponents{.id = 1, .components = {{0xff, 0x01}}});
    const auto                           packet = Net::makeSnapshot(scene);
    std::vector<Game::SEntityComponents> decoded;
    EXPECT_FALSE(Net::readSnapshot(packet.data(), packet.size(), decoded));
}

TEST(NetworkProtocol, UpdateRoundTripsPartialComponentsAndRemoval) {
    std::vector<std::uint8_t> position;
    SCPosition{12.5F, 42.0F}.serialize(position);
    const std::vector<Game::SUpdateEntity> updates{
        Game::SUpdateEntity{.id = 7, .components = {position}},
        Game::SUpdateEntity{.id = 9, .remove = true, .components = {}},
    };
    const auto                       packet = Net::makeUpdate(456, updates);
    std::vector<Game::SUpdateEntity> decoded;
    std::uint64_t                    tick = 0;
    ASSERT_TRUE(Net::readUpdate(packet.data(), packet.size(), decoded, tick));
    EXPECT_EQ(tick, 456U);
    ASSERT_EQ(decoded.size(), 2U);
    EXPECT_EQ(decoded.at(0).components.front(), position);
    EXPECT_TRUE(decoded.at(1).remove);
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

TEST(SceneData, UpdatesMergeComponentsAndRemoveEntities) {
    SSceneData                scene;
    std::vector<std::uint8_t> firstPosition;
    SCPosition{1.0F, 2.0F}.serialize(firstPosition);
    scene.replaceEntities({
        Game::SEntityComponents{.id = 7, .components = {firstPosition}},
        Game::SEntityComponents{.id = 9, .creationTick = 5, .components = {}},
    });
    std::vector<std::uint8_t> position;
    SCPosition{12.5F, 42.0F}.serialize(position);
    ASSERT_TRUE(scene.applyUpdates({
        Game::SUpdateEntity{.id = 7, .components = {position}},
        Game::SUpdateEntity{.id = 9, .remove = true, .components = {}},
    }));
    ASSERT_EQ(scene.entities.size(), 1U);
    EXPECT_EQ(scene.entities.front().id, 7U);
    ASSERT_EQ(scene.entities.front().components.size(), 1U);
    EXPECT_EQ(scene.entities.front().components.front(), position);
}

TEST(SceneData, UpdatesIntroduceNewEntities) {
    SSceneData                scene;
    std::vector<std::uint8_t> position;
    SCPosition{25.0F, 35.0F}.serialize(position);

    ASSERT_TRUE(scene.applyUpdates({
        Game::SUpdateEntity{.id = 11, .components = {position}},
    }));
    ASSERT_EQ(scene.entities.size(), 1U);
    EXPECT_EQ(scene.entities.front().id, 11U);
    ASSERT_EQ(scene.entities.front().components.size(), 1U);
}
