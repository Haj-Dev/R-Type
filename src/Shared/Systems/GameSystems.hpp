#pragma once

#include "Shared/ECS/ECS.hpp"
#include "Shared/Components/Collider.hpp"
#include "Shared/Components/EntityType.hpp"
#include "Shared/Components/Health.hpp"
#include "Shared/Components/Player.hpp"
#include "Shared/Components/Position.hpp"
#include "Shared/Components/Projectile.hpp"
#include "Shared/Components/Velocity.hpp"
#include "Shared/GameWorld.hpp"
#include "Shared/PlayerActions.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <toml++/impl/array.hpp>
#include <toml++/impl/forward_declarations.hpp>
#include <toml++/impl/parser.hpp>
#include <toml++/impl/table.hpp>
#include <unordered_set>
#include <vector>

namespace Game {

    class CPlayerSystem {
      public:
        void update(Ecs::CRegistry& registry, const SPLayerActions& actions, float delta) const {
            std::vector<SPlayerSpawn> projectiles;
            registry.each<SCPosition, SCPlayer>(
                [&](Ecs::SEntity, SCPosition& position, SCPlayer& player) {
                    const auto action = actions.forPlayer(player.id);
                    position.x = std::clamp(position.x + (action.horizontal() * PlayerSpeed * delta),
                                            20.0F, WorldWidth - 20.0F);
                    position.y = std::clamp(position.y + (action.vertical() * PlayerSpeed * delta),
                                            20.0F, WorldHeight - 20.0F);
                    player.fireCooldown = std::max(0.0F, player.fireCooldown - delta);
                    if (action.fire && player.fireCooldown <= 0.0F) {
                        projectiles.push_back(SPlayerSpawn{.position = position, .playerId = player.id});
                        player.fireCooldown = FireInterval;
                    }
                });

            for (const auto& projectile : projectiles)
                spawnProjectile(registry, projectile.position, projectile.playerId);
        }

        void ensureConnectedPlayers(Ecs::CRegistry& registry, const SPLayerActions& actions) const {
            for (std::uint8_t id = 0; id < SPLayerActions::MaxPlayers; ++id) {
                if (!actions.connected.at(id))
                    continue;
                bool exists = false;
                registry.each<SCPlayer>(
                    [&](Ecs::SEntity, SCPlayer& player) { exists = exists || player.id == id; });
                if (!exists)
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
            registry.emplace<SCPosition>(entity,
                                         SCPosition{100.0F, 180.0F + (static_cast<float>(id) * 120.0F)});
            registry.emplace<SCVelocity>(entity);
            registry.emplace<SCHealth>(entity, SCHealth{3});
            registry.emplace<SCCollider>(entity, SCCollider{18.0F});
            registry.emplace<SCEntityType>(entity, SCEntityType{eEntityKind::PLAYER});
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
        CEnemySpawn(const toml::table &tbl) {
            std::optional<float> optStart = tbl.at_path("start").value<float>();
            std::optional<uint> optCount = tbl.at_path("count").value<uint>();

            if (!optStart.has_value() || !optCount.has_value())
                throw std::runtime_error((std::ostringstream("Error: Failed to read value in ennemy: \n") << tbl).str());
            nextSpawnTime = optStart.value();
            count = optCount.value();
            type = tbl.at_path("type").value_or("linear");
            spawnSpeed = tbl.at_path("spawnSpeed").value_or(1);
            speed = tbl.at_path("speed").value_or(1);
            health = tbl.at_path("health").value_or(1);
            damage = tbl.at_path("damage").value_or(1);
            startY = tbl.at_path("startY").value_or(0);
            size = tbl.at_path("size").value_or(16);
            if (type == "column") {
                gapY = tbl.at_path("gapY").value_or(0);
                direction = std::string(tbl.at_path("direction").value_or("down")) == "up";
            }
        }

        void enemySpawned() {
            spawned++;
            nextSpawnTime += spawnSpeed;
        }

        bool toDelete() const noexcept {return spawned >= count;}

        [[nodiscard]] uint getSpawned() const noexcept {return spawned;}
        [[nodiscard]] uint getCount() const noexcept {return count;}
        [[nodiscard]] float getSpeed() const noexcept {return speed;}
        [[nodiscard]] int16_t getHealth() const noexcept {return health;}
        [[nodiscard]] int16_t getDamage() const noexcept {return damage;}
        [[nodiscard]] float getStartY() const noexcept {return startY;}
        [[nodiscard]] float getGapY() const noexcept {return gapY;}
        [[nodiscard]] std::string getType() const noexcept {return type;}
        [[nodiscard]] bool getDirection() const noexcept {return direction;}
        [[nodiscard]] float getNextSpawnTime() const noexcept {return nextSpawnTime;}
        [[nodiscard]] float getSize() const noexcept {return size;}

      private:
        uint spawned = 0;
        uint count;
        float spawnSpeed = 1;
        float speed = 1;
        uint size = 16;
        int16_t health = 1;
        int16_t damage = 1;
        uint startY = 0;
        uint gapY = 0;
        std::string type = "linear"; // linear || column
        bool direction = false; // up = true; down = false
        float nextSpawnTime;
    };

    class CEnemySpawnSystem {
      public:
        CEnemySpawnSystem(const std::string &filepath) {
            toml::parse_result result = toml::parse_file(filepath);
            if (!result) {
                std::cerr << "Error parsing file: " << result.error() << std::endl;
                return;
            }
            toml::table tbl = std::move(result);
            toml::array *array = tbl.at_path("enemies").as_array();
            if (array == nullptr)
                return;
            enemySpawns.clear();
            for (toml::node &tableEntry: *array)
                enemySpawns.emplace_back(*tableEntry.as_table());
        }

        void update(Ecs::CRegistry& registry, float delta) {
            timer_ += delta;

            for (CEnemySpawn &enemySpawn: enemySpawns) {
                if (enemySpawn.getNextSpawnTime() > timer_)
                    continue;
                const auto entity = registry.create();
                registry.emplace<SCPosition>(entity, SCPosition{WorldWidth + 20.0F, enemySpawn.getStartY() + (enemySpawn.getGapY() * enemySpawn.getSpawned())});
                registry.emplace<SCVelocity>(entity, SCVelocity{-(enemySpawn.getSpeed()), 0.0F});
                registry.emplace<SCHealth>(entity, SCHealth{enemySpawn.getHealth()});
                registry.emplace<SCCollider>(entity, SCCollider{enemySpawn.getSize()});
                registry.emplace<SCEntityType>(entity, SCEntityType{eEntityKind::ENEMY});
                enemySpawn.enemySpawned();
            }
            std::erase_if(enemySpawns, [](CEnemySpawn &enemySpawn) {return enemySpawn.toDelete();});
        }

      private:
        std::vector<CEnemySpawn> enemySpawns;
        float timer_ = 0.0F;
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
                    const auto* health = registry.get<SCHealth>(entity);
                    result.push_back(SSnapshotEntity{
                        .id     = entity.index,
                        .kind   = type.value,
                        .x      = position.x,
                        .y      = position.y,
                        .health = static_cast<std::int16_t>(health == nullptr ? 0 : health->value),
                    });
                });
            return result;
        }
    };

} // namespace Game
