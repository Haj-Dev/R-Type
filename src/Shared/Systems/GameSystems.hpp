#pragma once

#include "Shared/ECS/ECS.hpp"
#include "Shared/Components/Collider.hpp"
#include "Shared/Components/EntityType.hpp"
#include "Shared/Components/Health.hpp"
#include "Shared/Components/Player.hpp"
#include "Shared/Components/Position.hpp"
#include "Shared/Components/Projectile.hpp"
#include "Shared/Components/Velocity.hpp"
#include "Shared/Components/DrawableShape.hpp"
#include "Shared/Components/DrawableSprite.hpp"
#include "Shared/Components/DrawableSpriteAtlas.hpp"
#include "Shared/GameWorld.hpp"
#include "Shared/PlayerActions.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <toml++/toml.hpp>
#include <toml++/impl/array.hpp>
#include <toml++/impl/forward_declarations.hpp>
#include <toml++/impl/parser.hpp>
#include <toml++/impl/table.hpp>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Game {

    class CAnimationSystem {
      public:
        void update(Ecs::CRegistry& registry, float delta) const {
            registry.each<SCDrawableSpriteAtlas>(
                [delta](Ecs::SEntity, SCDrawableSpriteAtlas& sprite) { sprite.advance(delta); });
        }
    };

    class CPlayerSystem {
      public:
        void update(Ecs::CRegistry& registry, const SPLayerActions& actions, float delta) const {
            std::vector<SPlayerSpawn> projectiles;
            registry.each<SCPosition, SCPlayer>(
                [&](Ecs::SEntity entity, SCPosition& position, SCPlayer& player) {
                    auto* health = registry.get<SCHealth>(entity);
                    if (health != nullptr && health->value <= 0) {
                        if (player.respawnCooldown < 0.0F)
                            player.respawnCooldown = RespawnDelay;
                        player.respawnCooldown = std::max(0.0F, player.respawnCooldown - delta);
                        if (player.respawnCooldown <= 0.0F) {
                            health->value = PlayerStartingHealth;
                            position.x = PlayerSpawnX;
                            position.y = PlayerSpawnY +
                                (static_cast<float>(player.id) * PlayerSpawnSpacing);
                            player.fireCooldown = 0.0F;
                            player.damageCooldown = RespawnInvulnerability;
                            player.respawnCooldown = -1.0F;
                        }
                        return;
                    }
                    player.fireCooldown   = std::max(0.0F, player.fireCooldown - delta);
                    player.damageCooldown = std::max(0.0F, player.damageCooldown - delta);
                    const auto action     = actions.forPlayer(player.id);
                    position.x = std::clamp(position.x + (action.horizontal() * PlayerSpeed * delta),
                                            20.0F, WorldWidth - 20.0F);
                    position.y = std::clamp(position.y + (action.vertical() * PlayerSpeed * delta),
                                            20.0F, WorldHeight - 20.0F);
                    if (action.fire && player.fireCooldown <= 0.0F) {
                        projectiles.push_back(SPlayerSpawn{.position = position, .playerId = player.id});
                        player.fireCooldown = FireInterval;
                    }
                });

            for (const auto& projectile : projectiles)
                spawnProjectile(registry, projectile.position, projectile.playerId);
        }

        void synchronizeConnectedPlayers(Ecs::CRegistry& registry, const SPLayerActions& actions) const {
            std::vector<Ecs::SEntity>                    disconnected;
            std::array<bool, SPLayerActions::MaxPlayers> exists{};

            registry.each<SCPlayer>([&](Ecs::SEntity entity, SCPlayer& player) {
                if (player.id >= SPLayerActions::MaxPlayers || !actions.connected.at(player.id)) {
                    disconnected.push_back(entity);
                    return;
                }
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
        };

        static void spawnPlayer(Ecs::CRegistry& registry, std::uint8_t id) {
            const auto entity = registry.create();
            registry.emplace<SCPosition>(
                entity, SCPosition{PlayerSpawnX, PlayerSpawnY + (static_cast<float>(id) * PlayerSpawnSpacing)});
            registry.emplace<SCVelocity>(entity);
            registry.emplace<SCHealth>(entity, SCHealth{3});
            registry.emplace<SCCollider>(entity, SCCollider{18.0F});
            registry.emplace<SCEntityType>(entity, SCEntityType{eEntityKind::PLAYER});
            registry.emplace<SCDrawableSpriteAtlas>(
                entity,
                SCDrawableSpriteAtlas{"assets/player_atlas.png",
                                      SDimentions{.width = 32U, .height = 32U}, 2, 0.5F, id % 4U});
            registry.emplace<SCPlayer>(entity, SCPlayer{id});
        }

        static void spawnProjectile(Ecs::CRegistry& registry, const SCPosition& position,
                                    std::uint8_t playerId) {
            const auto entity = registry.create();
            registry.emplace<SCPosition>(entity, SCPosition{position.x + 20.0F, position.y});
            registry.emplace<SCVelocity>(entity, SCVelocity{ProjectileSpeed, 0.0F});
            registry.emplace<SCCollider>(entity, SCCollider{4.0F});
            registry.emplace<SCEntityType>(entity, SCEntityType{eEntityKind::PLAYER_PROJECTILE});
            registry.emplace<SCProjectile>(entity, SCProjectile{playerId, 1.0F});
            registry.emplace<SCDrawableSpriteAtlas>(
                entity,
                SCDrawableSpriteAtlas{"assets/r-typesheet3.gif",
                                      SDimentions{.width = 17U, .height = 17U}, 12, 0.25F, 0});
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
        explicit CEnemySpawnSystem(std::vector<CEnemySpawn> spawns) : enemySpawns(std::move(spawns)) {}

        explicit CEnemySpawnSystem(const std::string& filepath) {
            toml::table tbl;
            try {
                tbl = toml::parse_file(filepath);
            } catch (const toml::parse_error& err) {
                std::cerr << "Error parsing file: " << err << std::endl;
                return;
            }
            toml::array* array = tbl.at_path("enemies").as_array();
            if (array == nullptr)
                return;
            enemySpawns.clear();
            for (toml::node& tableEntry : *array)
                enemySpawns.emplace_back(*tableEntry.as_table());
        }

        static CEnemySpawnSystem deterministic() {
            return CEnemySpawnSystem({CEnemySpawn{1, 1.0F, 0.0F, 1, 100.0F, 16.0F}});
        }

        void update(Ecs::CRegistry& registry, float delta) {
            timer_ += delta;

            for (CEnemySpawn& enemySpawn : enemySpawns) {
                if (enemySpawn.getNextSpawnTime() > timer_)
                    continue;
                const auto entity = registry.create();
                if (enemySpawn.getType() == "random") {
                    std::random_device              rd;
                    std::mt19937                    gen(rd());
                    std::uniform_int_distribution<> dis(0,
                                                        Game::WorldHeight - (enemySpawn.getSize() * 2));
                    registry.emplace<SCPosition>(
                        entity, SCPosition{WorldWidth + 20.0F, (dis(gen) + enemySpawn.getSize())});
                } else {
                    registry.emplace<SCPosition>(
                        entity,
                        SCPosition{WorldWidth + 20.0F,
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
                                          SDimentions{.width = 32U, .height = 36U}, 5, 0.5F, 0});
                enemySpawn.enemySpawned();
            }
            std::erase_if(enemySpawns, [](CEnemySpawn& enemySpawn) { return enemySpawn.toDelete(); });
        }

      private:
        std::vector<CEnemySpawn> enemySpawns;
        float                    timer_ = 0.0F;
    };

    class CCollisionSystem {
      public:
        void update(Ecs::CRegistry& registry) const {
            std::vector<Ecs::SEntity> destroyed;
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

            std::unordered_set<Ecs::SEntity> uniqueDestroyed(destroyed.begin(), destroyed.end());
            for (const auto entity : uniqueDestroyed)
                registry.destroy(entity);
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
        [[nodiscard]] std::vector<SSnapshotEntity> capture(Ecs::CRegistry& registry) const {
            std::vector<SSnapshotEntity> result;
            result.reserve(registry.size());
            registry.each<SCPosition, SCEntityType>(
                [&](Ecs::SEntity entity, SCPosition& position, SCEntityType& type) {
                    const auto*     health = registry.get<SCHealth>(entity);
                    SSnapshotEntity snapshot{
                        .id       = entity.index,
                        .kind     = type.value,
                        .x        = position.x,
                        .y        = position.y,
                        .health   = static_cast<std::int16_t>(health == nullptr ? 0 : health->value),
                        .layer    = 0,
                        .scale    = 1.0F,
                        .offsetX  = 0.0F,
                        .offsetY  = 0.0F,
                        .rotation = 0.0F,
                        .texture  = {},
                    };
                    if (const auto* shape = registry.get<SCDrawableShape>(entity))
                        snapshot.layer = shape->layer;
                    if (const auto* sprite = registry.get<SCDrawableSprite>(entity)) {
                        snapshot.layer    = sprite->layer;
                        snapshot.scale    = sprite->scale;
                        snapshot.offsetX  = sprite->offset.x;
                        snapshot.offsetY  = sprite->offset.y;
                        snapshot.rotation = sprite->rotation;
                        snapshot.texture  = sprite->texture;
                    }
                    if (const auto* atlas = registry.get<SCDrawableSpriteAtlas>(entity)) {
                        snapshot.atlas         = true;
                        snapshot.baseDimension = atlas->baseDimension;
                        snapshot.frames        = atlas->frames;
                        snapshot.frameDuration = atlas->frameDuration;
                        snapshot.variant       = atlas->variant;
                        snapshot.layer         = atlas->layer;
                        snapshot.scale         = atlas->scale;
                        snapshot.offsetX       = atlas->offset.x;
                        snapshot.offsetY       = atlas->offset.y;
                        snapshot.rotation      = atlas->rotation;
                        snapshot.texture       = atlas->texture;
                    }
                    result.push_back(std::move(snapshot));
                });
            return result;
        }
    };

} // namespace Game
