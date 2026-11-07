#include "Shared/Components/ComponentRegistry.hpp"
#include "Shared/Systems/GameSystems.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <toml++/toml.hpp>

namespace {
    std::filesystem::path sourceRoot() {
        auto path = std::filesystem::current_path();
        while (!path.empty()) {
            if (std::filesystem::exists(path / "assets/maps/init.toml"))
                return path;
            const auto parent = path.parent_path();
            if (parent == path)
                break;
            path = parent;
        }
        throw std::runtime_error("could not locate the project assets directory");
    }

    Ecs::SEntity addPlayer(Ecs::CRegistry& registry, Game::eWeaponMode mode) {
        const auto player = registry.create();
        registry.emplace<SCPosition>(player, SCPosition{100.0F, 180.0F});
        registry.emplace<SCCollider>(player, SCCollider{18.0F});
        registry.emplace<SCHealth>(player, SCHealth{3});
        registry.emplace<SCEntityType>(player, SCEntityType{Game::eEntityKind::PLAYER});
        registry.emplace<SCPlayer>(player, SCPlayer{0});
        const float duration =
            mode == Game::eWeaponMode::STANDARD ? 0.0F : Game::CPlayerSystem::PowerUpDuration;
        registry.emplace<SCWeapon>(player, SCWeapon{mode, duration});
        return player;
    }

    Ecs::SEntity addPowerUp(Ecs::CRegistry& registry, float x, float y, Game::ePowerUpKind kind) {
        const auto powerUp = registry.create();
        registry.emplace<SCPosition>(powerUp, SCPosition{x, y});
        registry.emplace<SCCollider>(powerUp, SCCollider{16.0F});
        registry.emplace<SCEntityType>(powerUp, SCEntityType{Game::eEntityKind::POWER_UP});
        registry.emplace<SCPowerUp>(powerUp, SCPowerUp{kind});
        return powerUp;
    }
} // namespace

TEST(PowerUps, FixedPickupsLoadFromTheFirstLevel) {
    const auto              root = sourceRoot();
    Game::CEnemySpawnSystem campaign({
        (root / "assets/maps/init.toml").string(),
        (root / "assets/maps/level_2.toml").string(),
        (root / "assets/maps/level_3.toml").string(),
        (root / "assets/maps/level_4.toml").string(),
        (root / "assets/maps/level_5.toml").string(),
    });
    Ecs::CRegistry          registry;

    campaign.update(registry, 0.016F);

    std::vector<SCPosition> positions;
    registry.each<SCPosition, SCPowerUp>(
        [&positions](Ecs::SEntity, const SCPosition& position, const SCPowerUp&) {
            positions.push_back(position);
        });
    ASSERT_EQ(positions.size(), 2U);
    EXPECT_NE(std::ranges::find_if(positions,
                                   [](const SCPosition& position) {
                                       return position.x == 440.0F && position.y == 225.0F;
                                   }),
              positions.end());
    EXPECT_NE(std::ranges::find_if(positions,
                                   [](const SCPosition& position) {
                                       return position.x == 880.0F && position.y == 500.0F;
                                   }),
              positions.end());
}

TEST(PowerUps, EveryLevelDefinesValidFixedPickupLocations) {
    const auto                     root = sourceRoot();
    const std::vector<std::string> files{
        "init.toml", "level_2.toml", "level_3.toml", "level_4.toml", "level_5.toml",
    };

    for (const auto& file : files) {
        const auto  level    = toml::parse_file((root / "assets/maps" / file).string());
        const auto* powerUps = level.at_path("powerups").as_array();
        ASSERT_NE(powerUps, nullptr) << file;
        ASSERT_FALSE(powerUps->empty()) << file;
        for (const auto& node : *powerUps) {
            const auto* powerUp = node.as_table();
            ASSERT_NE(powerUp, nullptr) << file;
            const auto type = powerUp->at_path("type").value_or(std::string{});
            EXPECT_TRUE(type == "heavy" || type == "spread") << file;
            const auto x = powerUp->at_path("x").value_or(-1.0F);
            const auto y = powerUp->at_path("y").value_or(-1.0F);
            EXPECT_GE(x, 0.0F);
            EXPECT_LE(x, Game::WorldWidth);
            EXPECT_GE(y, 0.0F);
            EXPECT_LE(y, Game::WorldHeight);
        }
    }
}

TEST(PowerUps, OnlyServerCollectsAndAppliesPickup) {
    Ecs::CRegistry         registry;
    const auto             player  = addPlayer(registry, Game::eWeaponMode::STANDARD);
    const auto             powerUp = addPowerUp(registry, 110.0F, 180.0F, Game::ePowerUpKind::SPREAD);
    Game::CCollisionSystem collisions;

    collisions.update(registry, false);
    ASSERT_NE(registry.get<SCPowerUp>(powerUp), nullptr);
    ASSERT_NE(registry.get<SCWeapon>(player), nullptr);
    EXPECT_EQ(registry.get<SCWeapon>(player)->mode, Game::eWeaponMode::STANDARD);

    collisions.update(registry, true);
    EXPECT_EQ(registry.get<SCWeapon>(player)->mode, Game::eWeaponMode::SPREAD);
    EXPECT_FLOAT_EQ(registry.get<SCWeapon>(player)->durationRemaining,
                    Game::CPlayerSystem::PowerUpDuration);
    EXPECT_EQ(registry.get<SCPowerUp>(powerUp), nullptr);
}

TEST(PowerUps, WeaponReturnsToStandardWhenTimerExpires) {
    Ecs::CRegistry registry;
    const auto     player                             = addPlayer(registry, Game::eWeaponMode::HEAVY);
    registry.get<SCWeapon>(player)->durationRemaining = 2.0F;
    SPLayerActions      actions;
    Game::CPlayerSystem playerSystem;

    playerSystem.update(registry, actions, 1.5F);
    EXPECT_EQ(registry.get<SCWeapon>(player)->mode, Game::eWeaponMode::HEAVY);
    EXPECT_FLOAT_EQ(registry.get<SCWeapon>(player)->durationRemaining, 0.5F);

    playerSystem.update(registry, actions, 0.5F);
    EXPECT_EQ(registry.get<SCWeapon>(player)->mode, Game::eWeaponMode::STANDARD);
    EXPECT_FLOAT_EQ(registry.get<SCWeapon>(player)->durationRemaining, 0.0F);
}

TEST(PowerUps, CollectingAnotherPickupRefreshesDurationAndMode) {
    Ecs::CRegistry registry;
    const auto     player                             = addPlayer(registry, Game::eWeaponMode::HEAVY);
    registry.get<SCWeapon>(player)->durationRemaining = 1.0F;
    const auto powerUp = addPowerUp(registry, 110.0F, 180.0F, Game::ePowerUpKind::SPREAD);

    Game::CCollisionSystem{}.update(registry);

    EXPECT_EQ(registry.get<SCWeapon>(player)->mode, Game::eWeaponMode::SPREAD);
    EXPECT_FLOAT_EQ(registry.get<SCWeapon>(player)->durationRemaining,
                    Game::CPlayerSystem::PowerUpDuration);
    EXPECT_EQ(registry.get<SCPowerUp>(powerUp), nullptr);
}

TEST(PowerUps, HeavyWeaponMakesLargerHigherDamageProjectile) {
    Ecs::CRegistry registry;
    addPlayer(registry, Game::eWeaponMode::HEAVY);
    SPLayerActions actions;
    actions.states.at(0).fire = true;

    Game::CPlayerSystem{}.update(registry, actions, 0.016F);

    const auto projectiles = registry.view<SCProjectile>();
    ASSERT_NE(projectiles.begin(), projectiles.end());
    const auto projectile = *projectiles.begin();
    EXPECT_FLOAT_EQ(registry.get<SCProjectile>(projectile)->damage, 3.0F);
    EXPECT_FLOAT_EQ(registry.get<SCCollider>(projectile)->radius, 7.0F);
    EXPECT_FLOAT_EQ(registry.get<SCDrawableSpriteAtlas>(projectile)->scale, 1.5F);
}

TEST(PowerUps, HeavyProjectileDealsThreeDamageOnCollision) {
    Ecs::CRegistry registry;
    const auto     projectile = registry.create();
    registry.emplace<SCPosition>(projectile, SCPosition{200.0F, 200.0F});
    registry.emplace<SCCollider>(projectile, SCCollider{7.0F});
    registry.emplace<SCEntityType>(projectile, SCEntityType{Game::eEntityKind::PLAYER_PROJECTILE});
    registry.emplace<SCProjectile>(projectile, SCProjectile{0, 3.0F});
    const auto enemy = registry.create();
    registry.emplace<SCPosition>(enemy, SCPosition{205.0F, 200.0F});
    registry.emplace<SCCollider>(enemy, SCCollider{16.0F});
    registry.emplace<SCEntityType>(enemy, SCEntityType{Game::eEntityKind::ENEMY});
    registry.emplace<SCHealth>(enemy, SCHealth{5});

    Game::CCollisionSystem{}.update(registry);

    ASSERT_NE(registry.get<SCHealth>(enemy), nullptr);
    EXPECT_EQ(registry.get<SCHealth>(enemy)->value, 2);
}

TEST(PowerUps, SpreadWeaponFiresThreeAngledProjectiles) {
    Ecs::CRegistry registry;
    addPlayer(registry, Game::eWeaponMode::SPREAD);
    SPLayerActions actions;
    actions.states.at(0).fire = true;

    Game::CPlayerSystem{}.update(registry, actions, 0.016F);

    std::size_t projectileCount = 0;
    registry.each<SCProjectile>(
        [&projectileCount](Ecs::SEntity, const SCProjectile&) { ++projectileCount; });
    ASSERT_EQ(projectileCount, 3U);
    std::vector<float> verticalVelocities;
    registry.each<SCVelocity, SCProjectile>(
        [&verticalVelocities](Ecs::SEntity, const SCVelocity& velocity, const SCProjectile&) {
            verticalVelocities.push_back(velocity.y);
        });
    std::ranges::sort(verticalVelocities);
    EXPECT_FLOAT_EQ(verticalVelocities[0], -180.0F);
    EXPECT_FLOAT_EQ(verticalVelocities[1], 0.0F);
    EXPECT_FLOAT_EQ(verticalVelocities[2], 180.0F);
}

TEST(PowerUps, WeaponModeSurvivesComponentSerialization) {
    CComponentRegistry        components;
    std::vector<std::uint8_t> bytes;
    SCWeapon{Game::eWeaponMode::SPREAD, Game::CPlayerSystem::PowerUpDuration}.serialize(bytes);

    const auto  decoded = components.deserialize(bytes);
    const auto* weapon  = dynamic_cast<const SCWeapon*>(decoded.get());
    ASSERT_NE(weapon, nullptr);
    EXPECT_EQ(weapon->mode, Game::eWeaponMode::SPREAD);
    EXPECT_FLOAT_EQ(weapon->durationRemaining, Game::CPlayerSystem::PowerUpDuration);
}
