#pragma once

#include "Shared/Components/ComponentRegistry.hpp"
#include "Shared/ECS/ECS.hpp"
#include "Shared/Components/Collider.hpp"
#include "Shared/Components/EntityType.hpp"
#include "Shared/Components/Health.hpp"
#include "Shared/Components/LevelState.hpp"
#include "Shared/Components/Player.hpp"
#include "Shared/Components/Position.hpp"
#include "Shared/Components/Projectile.hpp"
#include "Shared/Components/Velocity.hpp"
#include "Shared/Components/Weapon.hpp"
#include "Shared/Components/DrawableSpriteAtlas.hpp"
#include "Shared/GameWorld.hpp"
#include "Shared/PlayerActions.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>
#include <toml++/impl/array.hpp>
#include <toml++/impl/forward_declarations.hpp>
#include <toml++/impl/parser.hpp>
#include <toml++/impl/table.hpp>
#include <unordered_set>
#include <utility>
#include <tuple>
#include <vector>

namespace Game {

    class CPlayerSystem {
      public:
        static constexpr float PowerUpDuration = 20.0F;

        void update(Ecs::CRegistry& registry, const SPLayerActions& actions, float delta) const {
            std::vector<SPlayerSpawn> projectiles;
            registry.each<SCPosition, SCPlayer>(
                [&](Ecs::SEntity entity, SCPosition& position, SCPlayer& player) {
                    const auto* health = registry.get<SCHealth>(entity);
                    if (health != nullptr && health->value <= 0)
                        return;
                    player.fireCooldown   = std::max(0.0F, player.fireCooldown - delta);
                    player.damageCooldown = std::max(0.0F, player.damageCooldown - delta);
                    if (auto* weapon = registry.get<SCWeapon>(entity);
                        weapon != nullptr && weapon->mode != eWeaponMode::STANDARD) {
                        weapon->durationRemaining = std::max(0.0F, weapon->durationRemaining - delta);
                        if (weapon->durationRemaining == 0.0F)
                            weapon->mode = eWeaponMode::STANDARD;
                    }
                    const auto action = actions.forPlayer(player.id);
                    position.x = std::clamp(position.x + (action.horizontal() * PlayerSpeed * delta),
                                            20.0F, WorldWidth - 20.0F);
                    position.y = std::clamp(position.y + (action.vertical() * PlayerSpeed * delta),
                                            20.0F, WorldHeight - 20.0F);
                    if (action.fire && player.fireCooldown <= 0.0F) {
                        const auto* weapon = registry.get<SCWeapon>(entity);
                        projectiles.push_back(SPlayerSpawn{
                            .position = position,
                            .playerId = player.id,
                            .mode     = weapon != nullptr ? weapon->mode : eWeaponMode::STANDARD,
                        });
                        player.fireCooldown = FireInterval;
                    }
                });

            for (const auto& projectile : projectiles) {
                if (projectile.mode == eWeaponMode::SPREAD) {
                    spawnProjectile(registry, projectile.position, projectile.playerId, projectile.mode,
                                    -180.0F, -20.0F);
                    spawnProjectile(registry, projectile.position, projectile.playerId, projectile.mode,
                                    0.0F, 0.0F);
                    spawnProjectile(registry, projectile.position, projectile.playerId, projectile.mode,
                                    180.0F, 20.0F);
                } else {
                    spawnProjectile(registry, projectile.position, projectile.playerId, projectile.mode);
                }
            }
        }

        void synchronizeConnectedPlayers(Ecs::CRegistry& registry, const SPLayerActions& actions,
                                         bool removeDisconnected = true) const {
            std::vector<Ecs::SEntity>                    disconnected;
            std::array<bool, SPLayerActions::MaxPlayers> exists{};

            registry.each<SCPlayer>([&](Ecs::SEntity entity, SCPlayer& player) {
                if (removeDisconnected &&
                    (player.id >= SPLayerActions::MaxPlayers || !actions.connected.at(player.id))) {
                    disconnected.push_back(entity);
                    return;
                }
                if (player.id < SPLayerActions::MaxPlayers)
                    exists.at(player.id) = true;
            });

            for (const auto entity : disconnected)
                registry.destroy(entity);

            for (std::uint8_t id = 0; id < SPLayerActions::MaxPlayers; ++id) {
                if (actions.connected.at(id) && !exists.at(id))
                    spawnPlayer(registry, id);
            }
        }

      private:
        struct SPlayerSpawn {
            SCPosition   position;
            std::uint8_t playerId;
            eWeaponMode  mode;
        };

        static void spawnPlayer(Ecs::CRegistry& registry, std::uint8_t id) {
            const auto entity = registry.create();
            registry.emplace<SCPosition>(entity,
                                         SCPosition{100.0F, 180.0F + (static_cast<float>(id) * 120.0F)});
            registry.emplace<SCVelocity>(entity);
            registry.emplace<SCHealth>(entity, SCHealth{3});
            registry.emplace<SCCollider>(entity, SCCollider{18.0F});
            registry.emplace<SCEntityType>(entity, SCEntityType{eEntityKind::PLAYER});
            registry.emplace<SCDrawableSpriteAtlas>(
                entity,
                SCDrawableSpriteAtlas{"assets/player_atlas.png",
                                      SDimentions{.width = 32U, .height = 32U}, 4, 30U, id % 4U});
            registry.emplace<SCPlayer>(entity, SCPlayer{id});
            registry.emplace<SCWeapon>(entity, SCWeapon{eWeaponMode::STANDARD});
        }

        static void spawnProjectile(Ecs::CRegistry& registry, const SCPosition& position,
                                    std::uint8_t playerId, eWeaponMode mode,
                                    float verticalVelocity = 0.0F, float rotation = 0.0F) {
            const auto entity = registry.create();
            registry.emplace<SCPosition>(entity, SCPosition{position.x + 20.0F, position.y});
            registry.emplace<SCVelocity>(entity, SCVelocity{ProjectileSpeed, verticalVelocity});
            const float damage = mode == eWeaponMode::HEAVY ? 3.0F : 1.0F;
            const float radius = mode == eWeaponMode::HEAVY ? 7.0F : 4.0F;
            const float scale =
                mode == eWeaponMode::HEAVY ? 1.5F : (mode == eWeaponMode::SPREAD ? 0.85F : 1.0F);
            registry.emplace<SCCollider>(entity, SCCollider{radius});
            registry.emplace<SCEntityType>(entity, SCEntityType{eEntityKind::PLAYER_PROJECTILE});
            registry.emplace<SCProjectile>(entity, SCProjectile{playerId, damage});
            registry.emplace<SCDrawableSpriteAtlas>(
                entity,
                SCDrawableSpriteAtlas{"assets/r-typesheet3.gif",
                                      SDimentions{.width = 17U, .height = 17U},
                                      12,
                                      15U,
                                      0,
                                      0,
                                      scale,
                                      {},
                                      rotation});
        }
    };

    class CMovementSystem {
      public:
        void update(Ecs::CRegistry& registry, float delta) const {
            registry.each<SCPosition, SCVelocity>(
                [delta](Ecs::SEntity, SCPosition& position, const SCVelocity& velocity) {
                    position.x += velocity.x * delta;
                    position.y += velocity.y * delta;
                });
        }
    };

    class CEnemySpawn {
      public:
        CEnemySpawn(std::uint32_t spawnCount, float spawnInterval, float enemySpeed,
                    std::int16_t enemyHealth, float enemyStartY, float enemySize) :
            count(spawnCount), spawnSpeed(spawnInterval), speed(enemySpeed), size(enemySize),
            health(enemyHealth), damage(1), startY(enemyStartY), gapY(0), type("linear"),
            direction(false), nextSpawnTime(0.0F) {}

        explicit CEnemySpawn(const toml::table& tbl) {
            nextSpawnTime = tbl.at_path("start").value_or(0);
            count         = tbl.at_path("count").value_or(1);
            type          = tbl.at_path("type").value_or("linear");
            spawnSpeed    = tbl.at_path("spawnSpeed").value_or(1);
            speed         = tbl.at_path("speed").value_or(50);
            health        = tbl.at_path("health").value_or(1);
            damage        = tbl.at_path("damage").value_or(1);
            startY        = tbl.at_path("startY").value_or(500);
            size          = tbl.at_path("size").value_or(16);
            if (type == "column") {
                gapY      = tbl.at_path("gapY").value_or(0);
                direction = std::string(tbl.at_path("direction").value_or("down")) == "up";
            }
        }

        void enemySpawned() {
            spawned++;
            nextSpawnTime += spawnSpeed;
        }

        bool toDelete() const noexcept {
            return spawned >= count;
        }

        [[nodiscard]] std::uint32_t getSpawned() const noexcept {
            return spawned;
        }
        [[nodiscard]] std::uint32_t getCount() const noexcept {
            return count;
        }
        [[nodiscard]] float getSpeed() const noexcept {
            return speed;
        }
        [[nodiscard]] int16_t getHealth() const noexcept {
            return health;
        }
        [[nodiscard]] int16_t getDamage() const noexcept {
            return damage;
        }
        [[nodiscard]] float getStartY() const noexcept {
            return startY;
        }
        [[nodiscard]] float getGapY() const noexcept {
            return gapY;
        }
        [[nodiscard]] std::string getType() const noexcept {
            return type;
        }
        [[nodiscard]] bool getDirection() const noexcept {
            return direction;
        }
        [[nodiscard]] float getNextSpawnTime() const noexcept {
            return nextSpawnTime;
        }
        [[nodiscard]] float getSize() const noexcept {
            return size;
        }

      private:
        std::uint32_t spawned = 0;
        std::uint32_t count;
        float         spawnSpeed;
        float         speed;
        std::uint32_t size;
        int16_t       health;
        int16_t       damage;
        std::uint32_t startY;
        std::uint32_t gapY;
        std::string   type;      // linear || column || random
        bool          direction; // up = true; down = false
        float         nextSpawnTime;
    };

    class CEnemySpawnSystem {
      public:
        explicit CEnemySpawnSystem(std::vector<CEnemySpawn> spawns) :
            levels_{std::move(spawns)}, levelPowerUps_(1), enemySpawns(levels_.front()),
            powerUps_(levelPowerUps_.front()) {}

        explicit CEnemySpawnSystem(std::vector<std::vector<CEnemySpawn>> levelWaves,
                                   float                                 transitionDuration = 3.0F) :
            levels_(std::move(levelWaves)), transitionDuration_(transitionDuration) {
            if (levels_.empty())
                throw std::invalid_argument("a campaign must contain at least one level");
            levelPowerUps_.resize(levels_.size());
            enemySpawns = levels_.front();
            powerUps_   = levelPowerUps_.front();
        }

        explicit CEnemySpawnSystem(const std::vector<std::string>& levelFiles) {
            if (levelFiles.empty())
                throw std::invalid_argument("a campaign must contain at least one level file");
            levels_.reserve(levelFiles.size());
            levelPowerUps_.reserve(levelFiles.size());
            for (const auto& levelFile : levelFiles) {
                auto level = parseLevelFile(levelFile);
                levels_.push_back(std::move(level.enemies));
                levelPowerUps_.push_back(std::move(level.powerUps));
            }
            enemySpawns = levels_.front();
            powerUps_   = levelPowerUps_.front();
        }

        static CEnemySpawnSystem campaign() {
            return CEnemySpawnSystem({
                "assets/maps/init.toml",
                "assets/maps/level_2.toml",
                "assets/maps/level_3.toml",
                "assets/maps/level_4.toml",
                "assets/maps/level_5.toml",
            });
        }

        static CEnemySpawnSystem deterministic() {
            return CEnemySpawnSystem({CEnemySpawn{1, 1.0F, 0.0F, 1, 100.0F, 16.0F}});
        }

        void update(Ecs::CRegistry& registry, float delta) {
            if (finished_)
                return;
            if (transitionRemaining_ > 0.0F) {
                transitionRemaining_ = std::max(0.0F, transitionRemaining_ - delta);
                if (transitionRemaining_ == 0.0F)
                    loadNextLevel(registry);
                return;
            }

            timer_ += delta;
            spawnPowerUps(registry);
            spawnReadyEnemies(registry);
            if (enemySpawns.empty() && !hasLivingEnemies(registry)) {
                if (hasNextLevel())
                    transitionRemaining_ = transitionDuration_;
                else
                    finished_ = true;
            }
        }

        [[nodiscard]] SCLevelState state() const {
            return SCLevelState{static_cast<std::uint8_t>(levelIndex_ + 1U),
                                static_cast<std::uint8_t>(levelCount()), transitionRemaining_,
                                finished_};
        }

        [[nodiscard]] std::size_t levelCount() const noexcept {
            return levels_.size();
        }

      private:
        struct SPowerUpSpawn {
            float        x;
            float        y;
            ePowerUpKind kind;
        };

        struct SLevelConfiguration {
            std::vector<CEnemySpawn>   enemies;
            std::vector<SPowerUpSpawn> powerUps;
        };

        static SLevelConfiguration parseLevelFile(const std::string& filepath) {
            const toml::table  tbl   = toml::parse_file(filepath);
            const toml::array* array = tbl.at_path("enemies").as_array();
            if (array == nullptr)
                throw std::runtime_error("level file has no [[enemies]] entries: " + filepath);
            SLevelConfiguration level;
            for (const toml::node& tableEntry : *array) {
                const toml::table* entry = tableEntry.as_table();
                if (entry == nullptr)
                    throw std::runtime_error("invalid enemy wave in " + filepath);
                level.enemies.emplace_back(*entry);
            }
            if (const toml::array* powerUps = tbl.at_path("powerups").as_array()) {
                for (const toml::node& tableEntry : *powerUps) {
                    const toml::table* entry = tableEntry.as_table();
                    if (entry == nullptr)
                        throw std::runtime_error("invalid power-up entry in " + filepath);
                    const std::string type = entry->at_path("type").value_or(std::string{});
                    const float       x    = entry->at_path("x").value_or(-1.0F);
                    const float       y    = entry->at_path("y").value_or(-1.0F);
                    if (!std::isfinite(x) || !std::isfinite(y) || x < 0.0F || x > WorldWidth ||
                        y < 0.0F || y > WorldHeight)
                        throw std::runtime_error("power-up position is outside the game world in " +
                                                 filepath);
                    if (type == "heavy")
                        level.powerUps.push_back({x, y, ePowerUpKind::HEAVY});
                    else if (type == "spread")
                        level.powerUps.push_back({x, y, ePowerUpKind::SPREAD});
                    else
                        throw std::runtime_error("unknown power-up type in " + filepath + ": " + type);
                }
            }
            return level;
        }

        void loadLevel(std::size_t index) {
            levelIndex_          = index;
            timer_               = 0.0F;
            transitionRemaining_ = 0.0F;
            enemySpawns          = levels_.at(index);
            powerUps_            = levelPowerUps_.at(index);
            powerUpsSpawned_     = false;
        }

        void loadNextLevel(Ecs::CRegistry& registry) {
            if (!hasNextLevel()) {
                finished_ = true;
                return;
            }
            std::vector<Ecs::SEntity> remainingPowerUps;
            registry.each<SCEntityType>(
                [&remainingPowerUps](Ecs::SEntity entity, const SCEntityType& type) {
                    if (type.value == eEntityKind::POWER_UP)
                        remainingPowerUps.push_back(entity);
                });
            for (const auto entity : remainingPowerUps)
                registry.destroy(entity);
            loadLevel(levelIndex_ + 1U);
        }

        [[nodiscard]] bool hasNextLevel() const noexcept {
            return levelIndex_ + 1U < levelCount();
        }

        static bool hasLivingEnemies(Ecs::CRegistry& registry) {
            bool found = false;
            registry.each<SCEntityType>([&found](Ecs::SEntity, const SCEntityType& type) {
                if (type.value == eEntityKind::ENEMY)
                    found = true;
            });
            return found;
        }

        void spawnPowerUps(Ecs::CRegistry& registry) {
            if (powerUpsSpawned_)
                return;
            for (const auto& powerUp : powerUps_) {
                const auto entity = registry.create();
                registry.emplace<SCPosition>(entity, SCPosition{powerUp.x, powerUp.y});
                registry.emplace<SCCollider>(entity, SCCollider{16.0F});
                registry.emplace<SCEntityType>(entity, SCEntityType{eEntityKind::POWER_UP});
                registry.emplace<SCPowerUp>(entity, SCPowerUp{powerUp.kind});
                const auto color = powerUp.kind == ePowerUpKind::HEAVY ? eDrawableColor::COLOR_MAGENTA :
                                                                         eDrawableColor::COLOR_CYAN;
                registry.emplace<SCDrawableShape>(
                    entity,
                    SCDrawableShape{eDrawableShape::CIRCLE, 13.0F, static_cast<std::uint8_t>(color), 1});
            }
            powerUpsSpawned_ = true;
        }

        void spawnReadyEnemies(Ecs::CRegistry& registry) {
            for (CEnemySpawn& enemySpawn : enemySpawns) {
                if (enemySpawn.getNextSpawnTime() > timer_)
                    continue;
                const auto entity = registry.create();
                if (enemySpawn.getType() == "random") {
                    std::random_device              rd;
                    std::mt19937                    gen(rd());
                    std::uniform_int_distribution<> dis(
                        0, static_cast<int>(Game::WorldHeight - (enemySpawn.getSize() * 2)));
                    registry.emplace<SCPosition>(
                        entity, SCPosition{WorldWidth + 100.0F, (dis(gen) + enemySpawn.getSize())});
                } else {
                    registry.emplace<SCPosition>(
                        entity,
                        SCPosition{WorldWidth + 100.0F,
                                   enemySpawn.getStartY() +
                                       (enemySpawn.getGapY() * enemySpawn.getSpawned())});
                }
                registry.emplace<SCVelocity>(entity, SCVelocity{-enemySpawn.getSpeed(), 0.0F});
                registry.emplace<SCHealth>(entity, SCHealth{enemySpawn.getHealth()});
                registry.emplace<SCCollider>(entity, SCCollider{enemySpawn.getSize()});
                registry.emplace<SCEntityType>(entity, SCEntityType{eEntityKind::ENEMY});
                registry.emplace<SCDrawableSpriteAtlas>(
                    entity,
                    SCDrawableSpriteAtlas{"assets/r-typesheet5.gif",
                                          SDimentions{.width = 32U, .height = 36U}, 5, 30U, 0});
                enemySpawn.enemySpawned();
            }
            std::erase_if(enemySpawns, [](CEnemySpawn& enemySpawn) { return enemySpawn.toDelete(); });
        }

      private:
        std::vector<std::vector<CEnemySpawn>>   levels_;
        std::vector<std::vector<SPowerUpSpawn>> levelPowerUps_;
        std::vector<CEnemySpawn>                enemySpawns;
        std::vector<SPowerUpSpawn>              powerUps_;
        std::size_t                             levelIndex_          = 0;
        float                                   timer_               = 0.0F;
        float                                   transitionDuration_  = 3.0F;
        float                                   transitionRemaining_ = 0.0F;
        bool                                    finished_            = false;
        bool                                    powerUpsSpawned_     = false;
    };

    class CCollisionSystem {
      public:
        void update(Ecs::CRegistry& registry, bool authoritative = true) const {
            std::vector<Ecs::SEntity>                                         destroyed;
            std::vector<std::tuple<Ecs::SEntity, Ecs::SEntity, ePowerUpKind>> collectedPowerUps;
            std::unordered_set<Ecs::SEntity>                                  collectedPowerUpEntities;
            registry.each<SCPosition, SCCollider, SCEntityType, SCProjectile>(
                [&](Ecs::SEntity projectile, SCPosition& projectilePosition,
                    SCCollider& projectileCollider, SCEntityType& projectileType,
                    SCProjectile& projectileData) {
                    if (projectileType.value != eEntityKind::PLAYER_PROJECTILE)
                        return;
                    registry.each<SCPosition, SCCollider, SCEntityType, SCHealth>(
                        [&](Ecs::SEntity enemy, SCPosition& enemyPosition, SCCollider& enemyCollider,
                            SCEntityType& enemyType, SCHealth& health) {
                            if (enemyType.value != eEntityKind::ENEMY)
                                return;
                            if (enemyPosition.x > WorldWidth)
                                return;
                            const float dx     = projectilePosition.x - enemyPosition.x;
                            const float dy     = projectilePosition.y - enemyPosition.y;
                            const float radius = projectileCollider.radius + enemyCollider.radius;
                            if ((dx * dx) + (dy * dy) <= radius * radius) {
                                health.value -=
                                    static_cast<std::int16_t>(std::max(1.0F, projectileData.damage));
                                destroyed.push_back(projectile);
                                if (health.value <= 0)
                                    destroyed.push_back(enemy);
                            }
                        });
                });

            registry.each<SCPosition, SCCollider, SCEntityType, SCHealth, SCPlayer>(
                [&](Ecs::SEntity playerEntity, SCPosition& playerPosition, SCCollider& playerCollider,
                    SCEntityType& playerType, SCHealth& playerHealth, SCPlayer& player) {
                    if (playerType.value != eEntityKind::PLAYER || playerHealth.value <= 0 ||
                        player.damageCooldown > 0.0F)
                        return;
                    registry.each<SCPosition, SCCollider, SCEntityType, SCHealth>(
                        [&](Ecs::SEntity enemyEntity, SCPosition& enemyPosition,
                            SCCollider& enemyCollider, SCEntityType& enemyType, SCHealth&) {
                            if (enemyType.value != eEntityKind::ENEMY || player.damageCooldown > 0.0F)
                                return;
                            const float dx     = playerPosition.x - enemyPosition.x;
                            const float dy     = playerPosition.y - enemyPosition.y;
                            const float radius = playerCollider.radius + enemyCollider.radius;
                            if ((dx * dx) + (dy * dy) <= radius * radius) {
                                playerHealth.value = static_cast<std::int16_t>(playerHealth.value - 1);
                                player.damageCooldown = 1.0F;
                                destroyed.push_back(enemyEntity);
                            }
                        });
                    (void)playerEntity;
                });

            if (authoritative) {
                registry.each<SCPosition, SCCollider, SCEntityType, SCHealth, SCPlayer>(
                    [&](Ecs::SEntity playerEntity, SCPosition& playerPosition,
                        SCCollider& playerCollider, SCEntityType& playerType, SCHealth& playerHealth,
                        SCPlayer&) {
                        if (playerType.value != eEntityKind::PLAYER || playerHealth.value <= 0)
                            return;
                        registry.each<SCPosition, SCCollider, SCEntityType, SCPowerUp>(
                            [&](Ecs::SEntity powerUpEntity, SCPosition& powerUpPosition,
                                SCCollider& powerUpCollider, SCEntityType& powerUpType,
                                SCPowerUp& powerUp) {
                                if (powerUpType.value != eEntityKind::POWER_UP ||
                                    collectedPowerUpEntities.contains(powerUpEntity))
                                    return;
                                const float dx     = playerPosition.x - powerUpPosition.x;
                                const float dy     = playerPosition.y - powerUpPosition.y;
                                const float radius = playerCollider.radius + powerUpCollider.radius;
                                if ((dx * dx) + (dy * dy) <= radius * radius) {
                                    collectedPowerUpEntities.insert(powerUpEntity);
                                    collectedPowerUps.emplace_back(playerEntity, powerUpEntity,
                                                                   powerUp.kind);
                                }
                            });
                    });
            }

            std::unordered_set<Ecs::SEntity> uniqueDestroyed(destroyed.begin(), destroyed.end());
            for (const auto entity : uniqueDestroyed)
                registry.destroy(entity);
            for (const auto& [collector, powerUpEntity, kind] : collectedPowerUps) {
                if (registry.get<SCPowerUp>(powerUpEntity) == nullptr ||
                    registry.get<SCPlayer>(collector) == nullptr)
                    continue;
                const auto mode = kind == ePowerUpKind::HEAVY ? eWeaponMode::HEAVY : eWeaponMode::SPREAD;
                if (auto* weapon = registry.get<SCWeapon>(collector)) {
                    weapon->mode              = mode;
                    weapon->durationRemaining = CPlayerSystem::PowerUpDuration;
                } else {
                    registry.emplace<SCWeapon>(collector,
                                               SCWeapon{mode, CPlayerSystem::PowerUpDuration});
                }
                registry.destroy(powerUpEntity);
            }
        }
    };

    class CCleanupSystem {
      public:
        void update(Ecs::CRegistry& registry) const {
            std::vector<Ecs::SEntity> expired;
            registry.each<SCPosition, SCEntityType>([&](Ecs::SEntity entity, SCPosition& position,
                                                        SCEntityType& type) {
                if ((type.value == eEntityKind::PLAYER_PROJECTILE && position.x > WorldWidth + 40.0F) ||
                    (type.value == eEntityKind::ENEMY && position.x < -40.0F))
                    expired.push_back(entity);
            });
            for (const auto entity : expired)
                registry.destroy(entity);
        }
    };

    class CSnapshotSystem {
      public:
        [[nodiscard]] std::vector<SEntityComponents> capture(Ecs::CRegistry& registry) const {
            std::vector<SEntityComponents> result;
            result.reserve(registry.size());
            CComponentRegistry components;
            registry.eachEntity([&](Ecs::SEntity entity) {
                SEntityComponents snapshot{
                    .id           = entity.index,
                    .creationTick = entity.creationTick,
                    .components   = {},
                };
                snapshot.components = components.serialize(registry, entity);
                result.push_back(std::move(snapshot));
            });
            return result;
        }
    };

} // namespace Game
