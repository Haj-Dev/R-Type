#pragma once

#include "Shared/Components/ComponentRegistry.hpp"
#include "Shared/ECS/ECS.hpp"
#include "Shared/GameWorld.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/Systems/GameSystems.hpp"

#include <cstdint>
#include <vector>

namespace Game {

    class CSimulation {
      public:
        CSimulation() : enemySpawnSystem_("assets/maps/init.toml") {}

        static CSimulation deterministic() {
            return CSimulation(CEnemySpawnSystem::deterministic());
        }

        void applyActions(const SPLayerActions& actions) {
            actions_ = actions;
        }

        void synchronize(const std::vector<SEntityComponents>& snapshot, std::uint8_t) {
            registry_.clear();
            CComponentRegistry components;
            for (const auto& entity : snapshot) {
                const auto handle = registry_.create();
                for (const auto& bytes : entity.components) {
                    components.apply(registry_, handle, bytes);
                }
            }
        }

        void reset() {
            *this = CSimulation{};
        }

        void tick(float delta) {
            playerSystem_.synchronizeConnectedPlayers(registry_, actions_);
            playerSystem_.update(registry_, actions_, delta);
            movementSystem_.update(registry_, delta);
            animationSystem_.update(registry_, delta);
            enemySpawnSystem_.update(registry_, delta);
            collisionSystem_.update(registry_);
            cleanupSystem_.update(registry_);
        }

        [[nodiscard]] std::vector<SEntityComponents> snapshot() {
            return snapshotSystem_.capture(registry_);
        }

      private:
        Ecs::CRegistry    registry_;
        SPLayerActions    actions_;
        CPlayerSystem     playerSystem_;
        CMovementSystem   movementSystem_;
        CAnimationSystem  animationSystem_;
        CEnemySpawnSystem enemySpawnSystem_;
        CCollisionSystem  collisionSystem_;
        CCleanupSystem    cleanupSystem_;
        CSnapshotSystem   snapshotSystem_;

        explicit CSimulation(CEnemySpawnSystem enemySpawnSystem) :
            enemySpawnSystem_(std::move(enemySpawnSystem)) {}
    };

} // namespace Game
