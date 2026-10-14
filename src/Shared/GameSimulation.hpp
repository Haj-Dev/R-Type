#pragma once

#include "Shared/ECS/ECS.hpp"
#include "Shared/GameWorld.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/Systems/GameSystems.hpp"

#include <vector>

namespace Game {

    class CSimulation {
      public:
        CSimulation() = default;

        void applyActions(const SPLayerActions& actions) {
            actions_ = actions;
        }

        void tick(float delta) {
            playerSystem_.synchronizeConnectedPlayers(registry_, actions_);
            playerSystem_.update(registry_, actions_, delta);
            movementSystem_.update(registry_, delta);
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
        CEnemySpawnSystem enemySpawnSystem_ = CEnemySpawnSystem("assets/maps/init.toml");
        CCollisionSystem  collisionSystem_;
        CCleanupSystem    cleanupSystem_;
        CSnapshotSystem   snapshotSystem_;
    };

} // namespace Game
