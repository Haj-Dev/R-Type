#include "Shared/Components/ComponentRegistry.hpp"
#include "Shared/GameSimulation.hpp"

#include <gtest/gtest.h>

#include <algorithm>

namespace {
    std::unique_ptr<AComponent> findComponent(const Game::SEntityComponents& entity,
                                              AComponent::eType              type) {
        CComponentRegistry registry;
        for (const auto& bytes : entity.components) {
            auto component = registry.deserialize(bytes);
            if (component && component->type() == type)
                return component;
        }
        return {};
    }

    bool hasKind(const Game::SEntityComponents& entity, Game::eEntityKind kind) {
        auto        component = findComponent(entity, AComponent::eType::ENTITY_TYPE);
        const auto* type      = dynamic_cast<const SCEntityType*>(component.get());
        return type != nullptr && type->value == kind;
    }

} // namespace

TEST(GameSimulation, SpawnsConnectedPlayerAndEnemy) {
    Game::CSimulation simulation = Game::CSimulation::deterministic();
    SPLayerActions    actions;
    actions.connected.at(0) = true;
    simulation.applyActions(actions);
    simulation.tick(0.016F);
    const auto entities = simulation.snapshot();
    EXPECT_EQ(
        std::count_if(entities.begin(), entities.end(),
                      [](const auto& entity) { return hasKind(entity, Game::eEntityKind::PLAYER); }),
        1);
    EXPECT_EQ(
        std::count_if(entities.begin(), entities.end(),
                      [](const auto& entity) { return hasKind(entity, Game::eEntityKind::ENEMY); }),
        1);
}

TEST(GameSimulation, ClientPredictionPreservesRemotePlayers) {
    Game::CSimulation server = Game::CSimulation::deterministic();
    SPLayerActions    remoteActions;
    remoteActions.connected.at(1) = true;
    server.applyActions(remoteActions);
    server.tick(0.016F, true);

    Game::CSimulation client = Game::CSimulation::deterministic();
    client.synchronize(server.snapshot(), 0);
    SPLayerActions localActions;
    localActions.connected.at(0) = true;
    client.applyActions(localActions);
    client.tick(0.016F, false);

    const auto entities = client.snapshot();
    EXPECT_EQ(
        std::count_if(entities.begin(), entities.end(),
                      [](const auto& entity) { return hasKind(entity, Game::eEntityKind::PLAYER); }),
        2);
}

TEST(GameSimulation, PlayerInputMovesAndFires) {
    Game::CSimulation simulation = Game::CSimulation::deterministic();
    SPLayerActions    actions;
    actions.connected.at(0)    = true;
    actions.states.at(0).right = true;
    actions.states.at(0).fire  = true;
    simulation.applyActions(actions);
    simulation.tick(0.2F);
    const auto entities = simulation.snapshot();
    EXPECT_TRUE(std::ranges::any_of(entities, [](const auto& entity) {
        return hasKind(entity, Game::eEntityKind::PLAYER_PROJECTILE);
    }));
}

TEST(GameSimulation, PlayerMovementIsClampedToWorld) {
    Game::CSimulation simulation = Game::CSimulation::deterministic();
    SPLayerActions    actions;
    actions.connected.at(0)   = true;
    actions.states.at(0).left = true;
    actions.states.at(0).up   = true;
    simulation.applyActions(actions);
    simulation.tick(10.0F);
    const auto entities = simulation.snapshot();
    EXPECT_EQ(
        std::count_if(entities.begin(), entities.end(),
                      [](const auto& entity) { return hasKind(entity, Game::eEntityKind::PLAYER); }),
        1);
}

TEST(GameSimulation, ProjectileCooldownPreventsRapidFire) {
    Game::CSimulation simulation = Game::CSimulation::deterministic();
    SPLayerActions    actions;
    actions.connected.at(0)   = true;
    actions.states.at(0).fire = true;
    simulation.applyActions(actions);
    simulation.tick(0.01F);
    simulation.tick(0.01F);
    const auto entities = simulation.snapshot();
    EXPECT_EQ(std::count_if(entities.begin(), entities.end(),
                            [](const auto& entity) {
                                return hasKind(entity, Game::eEntityKind::PLAYER_PROJECTILE);
                            }),
              1);
}
