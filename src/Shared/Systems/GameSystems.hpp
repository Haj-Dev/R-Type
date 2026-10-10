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

    class CEnemySpawnSystem {
      public:
        void update(Ecs::CRegistry& registry, float delta) {
            timer_ -= delta;
            if (timer_ > 0.0F)
                return;

            const auto entity = registry.create();
            registry.emplace<SCPosition>(entity, SCPosition{WorldWidth + 20.0F, 100.0F});
            registry.emplace<SCVelocity>(entity, SCVelocity{-90.0F, 0.0F});
            registry.emplace<SCHealth>(entity, SCHealth{1});
            registry.emplace<SCCollider>(entity, SCCollider{16.0F});
            registry.emplace<SCEntityType>(entity, SCEntityType{eEntityKind::ENEMY});
            timer_ += 2.0F;
        }

      private:
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
