#include "Shared/Components/DrawableSpriteAtlas.hpp"
#include "Shared/GameSimulation.hpp"
#include "Shared/SceneData.hpp"
#include "Shared/Systems/GameSystems.hpp"

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
    SSceneData scene;
    std::vector<std::uint8_t> atlasBytes;
    SCDrawableSpriteAtlas{"atlas.png", SDimentions{.width = 32U, .height = 24U}, 5U, 0.08F, 2U}
        .serialize(atlasBytes);
    scene.replaceEntities({Game::SEntityComponents{.id = 1U, .components = {atlasBytes}}});

    const auto  renderEntity = *scene.renderRegistry.view<SCDrawableSpriteAtlas>().begin();
    const auto* atlasComponent = scene.renderRegistry.get<SCDrawableSpriteAtlas>(renderEntity);
    ASSERT_NE(atlasComponent, nullptr);
    EXPECT_FLOAT_EQ(atlasComponent->baseDimension.width, 32.0F);
    EXPECT_FLOAT_EQ(atlasComponent->baseDimension.height, 24.0F);
    EXPECT_EQ(atlasComponent->frames, 5U);
    EXPECT_FLOAT_EQ(atlasComponent->frameDuration, 0.08F);
    EXPECT_EQ(atlasComponent->variant, 2U);
}

TEST(SpriteAtlas, PlayerUsesTwoFrameAtlas) {
    Game::CSimulation simulation = Game::CSimulation::deterministic();
    SPLayerActions    actions;
    actions.connected.at(0) = true;
    simulation.applyActions(actions);
    simulation.tick(0.016F);

    const auto entities = simulation.snapshot();
    EXPECT_TRUE(std::ranges::any_of(entities, [](const auto& entity) {
        return std::ranges::any_of(entity.components, [](const auto& bytes) {
            return !bytes.empty() &&
                static_cast<AComponent::eType>(bytes.front()) ==
                AComponent::eType::DRAWABLE_SPRITE_ATLAS;
        });
    }));
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
    SSceneData scene;
    std::vector<std::uint8_t> atlas;
    SCDrawableSpriteAtlas{"atlas.png", SDimentions{.width = 32U, .height = 32U}, 2U, 0.1F}
        .serialize(atlas);
    scene.replaceEntities({Game::SEntityComponents{.id = 7U, .components = {atlas}}});

    const auto firstEntity = *scene.renderRegistry.view<SCDrawableSpriteAtlas>().begin();
    auto*      firstAtlas  = scene.renderRegistry.get<SCDrawableSpriteAtlas>(firstEntity);
    ASSERT_NE(firstAtlas, nullptr);
    firstAtlas->advance(0.12F);
    const auto animationTime = firstAtlas->animationTime();

    scene.replaceEntities({Game::SEntityComponents{.id = 7U, .components = {atlas}}});

    const auto  secondEntity = *scene.renderRegistry.view<SCDrawableSpriteAtlas>().begin();
    const auto* secondAtlas  = scene.renderRegistry.get<SCDrawableSpriteAtlas>(secondEntity);
    ASSERT_NE(secondAtlas, nullptr);
    EXPECT_EQ(secondAtlas->currentFrame(), 1U);
    EXPECT_FLOAT_EQ(secondAtlas->animationTime(), animationTime);
}
