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
        CSimulation() : CSimulation(CEnemySpawnSystem::campaign()) {}

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
                const auto handle = registry_.createAt(entity.id);
                for (const auto& bytes : entity.components) {
                    components.apply(registry_, handle, bytes);
                }
            }
        }

        void reset() {
            *this = CSimulation{};
        }

        void tick(float delta, bool isServer = true) {
            playerSystem_.synchronizeConnectedPlayers(registry_, actions_, isServer);
            playerSystem_.update(registry_, actions_, delta);
            movementSystem_.update(registry_, delta);
            collisionSystem_.update(registry_, isServer);
            cleanupSystem_.update(registry_);
            if (isServer) {
                enemySpawnSystem_.update(registry_, delta);
                registry_.each<SCLevelState>(
                    [this](Ecs::SEntity, SCLevelState& state) { state = enemySpawnSystem_.state(); });
            }
        }

        [[nodiscard]] std::vector<SEntityComponents> snapshot() {
            return snapshotSystem_.capture(registry_);
        }

      private:
        Ecs::CRegistry    registry_;
        SPLayerActions    actions_;
        CPlayerSystem     playerSystem_;
        CMovementSystem   movementSystem_;
        CEnemySpawnSystem enemySpawnSystem_;
        CCollisionSystem  collisionSystem_;
        CCleanupSystem    cleanupSystem_;
        CSnapshotSystem   snapshotSystem_;

        explicit CSimulation(CEnemySpawnSystem enemySpawnSystem) :
            enemySpawnSystem_(std::move(enemySpawnSystem)) {
            const auto levelState = registry_.create();
            registry_.emplace<SCLevelState>(levelState, enemySpawnSystem_.state());
        }
    };

} // namespace Game
