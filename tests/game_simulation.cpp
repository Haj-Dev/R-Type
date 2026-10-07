#include "Shared/GameSimulation.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

TEST(GameSimulation, StartsWithPlayerAndSpawnsEnemy) {
    Game::CSimulation simulation;

    simulation.tick(0.016F);
    const auto entities = simulation.snapshot();

    EXPECT_EQ(std::count_if(entities.begin(), entities.end(), [](const auto& entity) {
                  return entity.kind == Game::eEntityKind::PLAYER;
              }),
              1);
    EXPECT_EQ(std::count_if(entities.begin(), entities.end(), [](const auto& entity) {
                  return entity.kind == Game::eEntityKind::ENEMY;
              }),
              1);
}

TEST(GameSimulation, PlayerInputMovesAndFires) {
    Game::CSimulation simulation;
    SPLayerActions actions;
    actions.states.at(0).right = true;
    actions.states.at(0).fire = true;
    simulation.applyActions(actions);

    simulation.tick(0.2F);
    const auto entities = simulation.snapshot();

    const auto player = std::ranges::find_if(
        entities, [](const auto& entity) { return entity.kind == Game::eEntityKind::PLAYER; });
    ASSERT_NE(player, entities.end());
    EXPECT_GT(player->x, 100.0F);
    EXPECT_TRUE(std::ranges::any_of(entities, [](const auto& entity) {
        return entity.kind == Game::eEntityKind::PLAYER_PROJECTILE;
    }));
}

TEST(GameSimulation, ConnectedPlayersGetSeparateEntities) {
    Game::CSimulation simulation;
    SPLayerActions actions;
    actions.connected.at(0) = true;
    actions.connected.at(1) = true;
    simulation.applyActions(actions);
    simulation.tick(0.016F);

    const auto entities = simulation.snapshot();
    std::vector<Game::SSnapshotEntity> players;
    for (const auto& entity : entities) {
        if (entity.kind == Game::eEntityKind::PLAYER)
            players.push_back(entity);
    }

    ASSERT_EQ(players.size(), 2U);
    EXPECT_NE(players.at(0).id, players.at(1).id);
    EXPECT_NE(players.at(0).y, players.at(1).y);
}

TEST(GameSimulation, PlayerMovementIsClampedToWorld) {
    Game::CSimulation simulation;
    SPLayerActions actions;
    actions.states.at(0).left = true;
    actions.states.at(0).up = true;
    simulation.applyActions(actions);

    simulation.tick(10.0F);
    const auto entities = simulation.snapshot();
    const auto player = std::ranges::find_if(
        entities, [](const auto& entity) { return entity.kind == Game::eEntityKind::PLAYER; });

    ASSERT_NE(player, entities.end());
    EXPECT_FLOAT_EQ(player->x, 20.0F);
    EXPECT_FLOAT_EQ(player->y, 20.0F);
}

TEST(GameSimulation, ProjectileCooldownPreventsRapidFire) {
    Game::CSimulation simulation;
    SPLayerActions actions;
    actions.states.at(0).fire = true;
    simulation.applyActions(actions);

    simulation.tick(0.01F);
    simulation.tick(0.01F);
    const auto entities = simulation.snapshot();

    EXPECT_EQ(std::count_if(entities.begin(), entities.end(), [](const auto& entity) {
                  return entity.kind == Game::eEntityKind::PLAYER_PROJECTILE;
              }),
              1);
}

TEST(GameSimulation, ProjectilesAreCleanedUp) {
    Game::CSimulation simulation;
    SPLayerActions firing;
    firing.states.at(0).fire = true;
    simulation.applyActions(firing);
    simulation.tick(0.016F);

    SPLayerActions idle;
    simulation.applyActions(idle);
    for (int i = 0; i < 240; ++i)
        simulation.tick(0.016F);

    const auto entities = simulation.snapshot();
    EXPECT_FALSE(std::ranges::any_of(entities, [](const auto& entity) {
        return entity.kind == Game::eEntityKind::PLAYER_PROJECTILE;
    }));
}

TEST(GameSimulation, ProjectileCanDestroyEnemy) {
    Game::CSimulation simulation;
    SPLayerActions moveUp;
    moveUp.states.at(0).up = true;
    simulation.applyActions(moveUp);
    for (int i = 0; i < 20; ++i)
        simulation.tick(0.016F);

    SPLayerActions firing;
    firing.states.at(0).fire = true;
    simulation.applyActions(firing);
    for (int i = 0; i < 140; ++i)
        simulation.tick(0.016F);

    const auto entities = simulation.snapshot();
    EXPECT_EQ(std::count_if(entities.begin(), entities.end(), [](const auto& entity) {
                  return entity.kind == Game::eEntityKind::ENEMY;
              }),
              1);
}
