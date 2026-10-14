#include "Shared/Components/DrawableSpriteAtlas.hpp"
#include "Shared/GameSimulation.hpp"
#include "Shared/SceneData.hpp"
#include "Shared/Systems/GameSystems.hpp"
#include "Shared/Types.hpp"

#include <gtest/gtest.h>

#include <algorithm>

TEST(SpriteAtlas, AdvancesFramesAndPreservesOverflow) {
    Ecs::CRegistry registry;
    const auto     entity = registry.create();
    registry.emplace<SCDrawableSpriteAtlas>(
        entity, SCDrawableSpriteAtlas{"atlas.png", SDimentions{.width = 16U, .height = 16U}, 4U, 0.25F});

    Game::CAnimationSystem animation;
    animation.update(registry, 0.5F);

    const auto* atlas = registry.get<SCDrawableSpriteAtlas>(entity);
    ASSERT_NE(atlas, nullptr);
    EXPECT_EQ(atlas->currentFrame(), 2U);
    animation.update(registry, 0.25F);
    EXPECT_EQ(atlas->currentFrame(), 3U);
    animation.update(registry, 0.25F);
    EXPECT_EQ(atlas->currentFrame(), 0U);
}

TEST(SpriteAtlas, ZeroDurationRemainsStatic) {
    SCDrawableSpriteAtlas atlas{"atlas.png", SDimentions{.width = 16U, .height = 16U}, 4U};

    atlas.advance(1.0F);

    EXPECT_EQ(atlas.currentFrame(), 0U);
}

TEST(SpriteAtlas, SceneProjectionPreservesAtlasMetadata) {
    SSceneData            scene;
    Game::SSnapshotEntity snapshot;
    snapshot.texture       = "atlas.png";
    snapshot.atlas         = true;
    snapshot.baseDimension = SDimentions{.width = 32U, .height = 24U};
    snapshot.frames        = 5U;
    snapshot.frameDuration = 0.08F;
    snapshot.variant       = 2U;
    scene.replaceEntities({snapshot});

    const auto  renderEntity = *scene.renderRegistry.view<SCPosition>().begin();
    const auto* atlas        = scene.renderRegistry.get<SCDrawableSpriteAtlas>(renderEntity);
    ASSERT_NE(atlas, nullptr);
    EXPECT_FLOAT_EQ(atlas->baseDimension.width, 32.0F);
    EXPECT_FLOAT_EQ(atlas->baseDimension.height, 24.0F);
    EXPECT_EQ(atlas->frames, 5U);
    EXPECT_FLOAT_EQ(atlas->frameDuration, 0.08F);
    EXPECT_EQ(atlas->variant, 2U);
}

TEST(SpriteAtlas, PlayerUsesTwoFrameAtlas) {
    Game::CSimulation simulation;
    SPLayerActions    actions;
    actions.connected.at(0) = true;
    simulation.applyActions(actions);
    simulation.tick(0.016F);

    const auto entities = simulation.snapshot();
    const auto player   = std::ranges::find_if(
        entities, [](const auto& entity) { return entity.kind == Game::eEntityKind::PLAYER; });
    ASSERT_NE(player, entities.end());
    EXPECT_TRUE(player->atlas);
    EXPECT_FLOAT_EQ(player->baseDimension.width, 32.0F);
    EXPECT_FLOAT_EQ(player->baseDimension.height, 32.0F);
    EXPECT_EQ(player->frames, 2U);
}

TEST(SpriteAtlas, SnapshotRefreshPreservesAnimationState) {
    SSceneData            scene;
    Game::SSnapshotEntity snapshot;
    snapshot.id            = 7U;
    snapshot.texture       = "atlas.png";
    snapshot.atlas         = true;
    snapshot.baseDimension = SDimentions{.width = 32U, .height = 32U};
    snapshot.frames        = 2U;
    snapshot.frameDuration = 0.1F;
    scene.replaceEntities({snapshot});

    const auto firstEntity = *scene.renderRegistry.view<SCPosition>().begin();
    auto*      firstAtlas  = scene.renderRegistry.get<SCDrawableSpriteAtlas>(firstEntity);
    ASSERT_NE(firstAtlas, nullptr);
    firstAtlas->advance(0.12F);
    const auto animationTime = firstAtlas->animationTime();

    scene.replaceEntities({snapshot});

    const auto  secondEntity = *scene.renderRegistry.view<SCPosition>().begin();
    const auto* secondAtlas  = scene.renderRegistry.get<SCDrawableSpriteAtlas>(secondEntity);
    ASSERT_NE(secondAtlas, nullptr);
    EXPECT_EQ(secondAtlas->currentFrame(), 1U);
    EXPECT_FLOAT_EQ(secondAtlas->animationTime(), animationTime);
}
