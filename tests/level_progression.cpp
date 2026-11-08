#include "Shared/Components/LevelState.hpp"
#include "Shared/GameSimulation.hpp"
#include "Shared/Systems/GameSystems.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <toml++/toml.hpp>

namespace {
    struct SLevelChallenge {
        std::uint32_t enemies               = 0;
        std::uint32_t totalHealth           = 0;
        float         fastestEnemy          = 0.0F;
        float         shortestSpawnInterval = 0.0F;
    };

    std::filesystem::path sourceRoot() {
        auto path = std::filesystem::current_path();
        while (!path.empty()) {
            if (std::filesystem::exists(path / "assets/maps/level_1.toml"))
                return path;
            const auto parent = path.parent_path();
            if (parent == path)
                break;
            path = parent;
        }
        throw std::runtime_error("could not locate the project assets directory");
    }
} // namespace

TEST(LevelProgression, CampaignLoadsAllFiveLevelFiles) {
    const auto                     root = sourceRoot();
    const std::vector<std::string> files{
        (root / "assets/maps/level_1.toml").string(), (root / "assets/maps/level_2.toml").string(),
        (root / "assets/maps/level_3.toml").string(), (root / "assets/maps/level_4.toml").string(),
        (root / "assets/maps/level_5.toml").string(),
    };
    Game::CEnemySpawnSystem campaign(files);

    EXPECT_EQ(campaign.levelCount(), 5U);
    EXPECT_EQ(campaign.state().level, 1U);
    EXPECT_EQ(campaign.state().totalLevels, 5U);

    std::vector<SLevelChallenge> challenges;
    for (const auto& file : files) {
        const auto         level = toml::parse_file(file);
        const toml::array* waves = level.at_path("enemies").as_array();
        ASSERT_NE(waves, nullptr);
        SLevelChallenge challenge{.shortestSpawnInterval = std::numeric_limits<float>::max()};
        for (const auto& waveNode : *waves) {
            const auto* wave = waveNode.as_table();
            ASSERT_NE(wave, nullptr);
            const auto count  = wave->at_path("count").value_or(std::uint32_t{1});
            const auto health = wave->at_path("health").value_or(std::int16_t{1});
            challenge.enemies += count;
            challenge.totalHealth += count * static_cast<std::uint32_t>(health);
            challenge.fastestEnemy =
                std::max(challenge.fastestEnemy, wave->at_path("speed").value_or(0.0F));
            challenge.shortestSpawnInterval =
                std::min(challenge.shortestSpawnInterval,
                         wave->at_path("spawnSpeed").value_or(std::numeric_limits<float>::max()));
        }
        challenges.push_back(challenge);
    }

    for (std::size_t index = 1; index < challenges.size(); ++index) {
        EXPECT_GT(challenges[index].enemies, challenges[index - 1].enemies);
        EXPECT_GT(challenges[index].totalHealth, challenges[index - 1].totalHealth);
        EXPECT_GT(challenges[index].fastestEnemy, challenges[index - 1].fastestEnemy);
        EXPECT_LT(challenges[index].shortestSpawnInterval, challenges[index - 1].shortestSpawnInterval);
    }
}

TEST(LevelProgression, WaitsForClearAndTransitionsBetweenLevels) {
    Game::CEnemySpawnSystem progression({{}, {}}, 1.0F);
    Ecs::CRegistry          registry;

    progression.update(registry, 0.016F);
    auto state = progression.state();
    EXPECT_EQ(state.level, 1U);
    EXPECT_FLOAT_EQ(state.transitionRemaining, 1.0F);
    EXPECT_FALSE(state.finished);

    progression.update(registry, 0.5F);
    EXPECT_FLOAT_EQ(progression.state().transitionRemaining, 0.5F);

    progression.update(registry, 0.5F);
    state = progression.state();
    EXPECT_EQ(state.level, 2U);
    EXPECT_FLOAT_EQ(state.transitionRemaining, 0.0F);
    EXPECT_FALSE(state.finished);

    progression.update(registry, 0.016F);
    state = progression.state();
    EXPECT_EQ(state.level, 2U);
    EXPECT_TRUE(state.finished);

    progression.update(registry, 10.0F);
    state = progression.state();
    EXPECT_EQ(state.level, 2U);
    EXPECT_TRUE(state.finished);
}

TEST(LevelProgression, DoesNotAdvanceWhileEnemiesRemain) {
    Game::CEnemySpawnSystem progression({{}, {}}, 1.0F);
    Ecs::CRegistry          registry;
    const auto              enemy = registry.create();
    registry.emplace<SCEntityType>(enemy, SCEntityType{Game::eEntityKind::ENEMY});

    progression.update(registry, 0.016F);

    const auto state = progression.state();
    EXPECT_EQ(state.level, 1U);
    EXPECT_FLOAT_EQ(state.transitionRemaining, 0.0F);
    EXPECT_FALSE(state.finished);
}

TEST(LevelProgression, LevelStateSurvivesSerialization) {
    SCLevelState              expected{3U, 5U, 2.5F, false};
    std::vector<std::uint8_t> bytes;
    expected.serialize(bytes);

    CComponentRegistry components;
    const auto         decoded = components.deserialize(bytes);
    const auto*        actual  = dynamic_cast<const SCLevelState*>(decoded.get());
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual->level, 3U);
    EXPECT_EQ(actual->totalLevels, 5U);
    EXPECT_FLOAT_EQ(actual->transitionRemaining, 2.5F);
    EXPECT_FALSE(actual->finished);
}

TEST(LevelProgression, SnapshotCarriesAuthoritativeLevelToClient) {
    Game::CSimulation server = Game::CSimulation::deterministic();
    server.tick(0.016F);
    const auto        serverSnapshot = server.snapshot();

    Game::CSimulation client = Game::CSimulation::deterministic();
    client.synchronize(serverSnapshot, 0);
    client.tick(5.0F, false);

    const auto         clientSnapshot = client.snapshot();
    CComponentRegistry components;
    for (const auto& entity : clientSnapshot) {
        for (const auto& bytes : entity.components) {
            const auto  component = components.deserialize(bytes);
            const auto* state     = dynamic_cast<const SCLevelState*>(component.get());
            if (state == nullptr)
                continue;
            EXPECT_EQ(state->level, 1U);
            EXPECT_EQ(state->totalLevels, 1U);
            EXPECT_FALSE(state->finished);
            return;
        }
    }
    FAIL() << "server snapshot did not include level state";
}

TEST(LevelProgression, FinishedCampaignStopsClientSimulation) {
    Game::SEntityComponents levelEntity;
    levelEntity.id = 1;
    SCLevelState finishedState{1U, 1U, 0.0F, true};
    finishedState.serialize(levelEntity.components.emplace_back());

    Game::SEntityComponents playerEntity;
    playerEntity.id = 2;
    SCPosition position{100.0F, 180.0F};
    position.serialize(playerEntity.components.emplace_back());
    SCPlayer player{0};
    player.serialize(playerEntity.components.emplace_back());
    SCEntityType type{Game::eEntityKind::PLAYER};
    type.serialize(playerEntity.components.emplace_back());

    Game::CSimulation simulation = Game::CSimulation::deterministic();
    simulation.synchronize({levelEntity, playerEntity}, 0);
    SPLayerActions actions;
    actions.connected[0]    = true;
    actions.states[0].right = true;
    simulation.applyActions(actions);
    simulation.tick(1.0F, false);

    CComponentRegistry components;
    for (const auto& entity : simulation.snapshot()) {
        for (const auto& bytes : entity.components) {
            const auto  component = components.deserialize(bytes);
            const auto* actual    = dynamic_cast<const SCPosition*>(component.get());
            if (actual != nullptr && entity.id == playerEntity.id) {
                EXPECT_FLOAT_EQ(actual->x, 100.0F);
                EXPECT_FLOAT_EQ(actual->y, 180.0F);
                return;
            }
        }
    }
    FAIL() << "finished simulation snapshot did not include the player position";
}
