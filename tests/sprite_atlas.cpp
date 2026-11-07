#include "Shared/Components/DrawableSpriteAtlas.hpp"
#include "Shared/GameSimulation.hpp"
#include "Shared/SceneData.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

TEST(SpriteAtlas, SelectsFrameFromCurrentTick) {
    SCDrawableSpriteAtlas atlas{"atlas.png", SDimentions{.width = 16U, .height = 16U}, 4U, 15U};

    EXPECT_EQ(atlas.currentFrame(0U), 0U);
    EXPECT_EQ(atlas.currentFrame(14U), 0U);
    EXPECT_EQ(atlas.currentFrame(15U), 1U);
    EXPECT_EQ(atlas.currentFrame(60U), 0U);
}

TEST(SpriteAtlas, ZeroDurationRemainsStatic) {
    SCDrawableSpriteAtlas atlas{"atlas.png", SDimentions{.width = 16U, .height = 16U}, 4U};

    EXPECT_EQ(atlas.currentFrame(100U), 0U);
}

TEST(SpriteAtlas, SceneProjectionPreservesAtlasMetadata) {
    SSceneData                scene;
    std::vector<std::uint8_t> atlasBytes;
    SCDrawableSpriteAtlas{"atlas.png", SDimentions{.width = 32U, .height = 24U}, 5U, 5U, 2U}.serialize(
        atlasBytes);
    scene.replaceEntities({Game::SEntityComponents{.id = 1U, .components = {atlasBytes}}});

    const auto  renderEntity   = *scene.renderRegistry.view<SCDrawableSpriteAtlas>().begin();
    const auto* atlasComponent = scene.renderRegistry.get<SCDrawableSpriteAtlas>(renderEntity);
    ASSERT_NE(atlasComponent, nullptr);
    EXPECT_FLOAT_EQ(atlasComponent->baseDimension.width, 32.0F);
    EXPECT_FLOAT_EQ(atlasComponent->baseDimension.height, 24.0F);
    EXPECT_EQ(atlasComponent->frames, 5U);
    EXPECT_EQ(atlasComponent->frameDuration, 5U);
    EXPECT_EQ(atlasComponent->variant, 2U);
}

TEST(SpriteAtlas, PlayersUseFourFrameAtlasWithDistinctVariants) {
    Game::CSimulation simulation = Game::CSimulation::deterministic();
    SPLayerActions    actions;
    actions.connected.fill(true);
    simulation.applyActions(actions);
    simulation.tick(0.016F);

    SSceneData scene;
    scene.replaceEntities(simulation.snapshot());

    std::array<bool, SPLayerActions::MaxPlayers> foundPlayers{};
    scene.renderRegistry.each<SCPlayer, SCDrawableSpriteAtlas>(
        [&foundPlayers](Ecs::SEntity, const SCPlayer& player, const SCDrawableSpriteAtlas& atlas) {
            ASSERT_LT(player.id, SPLayerActions::MaxPlayers);
            foundPlayers.at(player.id) = true;
            EXPECT_EQ(atlas.texture, "assets/player_atlas.png");
            EXPECT_FLOAT_EQ(atlas.baseDimension.width, 32.0F);
            EXPECT_FLOAT_EQ(atlas.baseDimension.height, 32.0F);
            EXPECT_EQ(atlas.frames, 4U);
            EXPECT_EQ(atlas.frameDuration, 30U);
            EXPECT_EQ(atlas.variant, player.id % 4U);
        });

    EXPECT_TRUE(std::ranges::all_of(foundPlayers, [](bool found) { return found; }));
}

TEST(SpriteAtlas, EnemyUsesFiveFrameAtlas) {
    Game::CSimulation simulation = Game::CSimulation::deterministic();
    simulation.tick(1.0F);

    const auto entities = simulation.snapshot();
    EXPECT_TRUE(std::ranges::any_of(entities, [](const auto& entity) {
        return std::ranges::any_of(entity.components, [](const auto& bytes) {
            return !bytes.empty() &&
                static_cast<AComponent::eType>(bytes.front()) ==
                AComponent::eType::DRAWABLE_SPRITE_ATLAS;
        });
    }));
}

TEST(SpriteAtlas, SnapshotRefreshPreservesAnimationState) {
    SSceneData                scene;
    std::vector<std::uint8_t> atlas;
    SCDrawableSpriteAtlas{"atlas.png", SDimentions{.width = 32U, .height = 32U}, 2U, 6U}.serialize(
        atlas);
    scene.replaceEntities({Game::SEntityComponents{.id = 7U, .components = {atlas}}});

    const auto firstEntity = *scene.renderRegistry.view<SCDrawableSpriteAtlas>().begin();
    auto*      firstAtlas  = scene.renderRegistry.get<SCDrawableSpriteAtlas>(firstEntity);
    ASSERT_NE(firstAtlas, nullptr);
    scene.replaceEntities({Game::SEntityComponents{.id = 7U, .components = {atlas}}});

    const auto  secondEntity = *scene.renderRegistry.view<SCDrawableSpriteAtlas>().begin();
    const auto* secondAtlas  = scene.renderRegistry.get<SCDrawableSpriteAtlas>(secondEntity);
    ASSERT_NE(secondAtlas, nullptr);
    EXPECT_EQ(secondAtlas->currentFrame(scene.tick), 0U);
    EXPECT_EQ(secondAtlas->currentFrame(6U), 1U);
}
