#pragma once

#include "Shared/ECS/ECS.hpp"
#include "Shared/GameWorld.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/Systems/GameSystems.hpp"

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

        // Rebuilds the world and systems for a fresh game session.
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

        [[nodiscard]] std::vector<SSnapshotEntity> snapshot() {
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
