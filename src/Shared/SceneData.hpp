#pragma once

#include "Shared/ECS/ECS.hpp"
#include "Shared/Components/Drawable.hpp"
#include "Shared/Components/EntityType.hpp"
#include "Shared/Components/Health.hpp"
#include "Shared/Components/Position.hpp"
#include "Shared/GameWorld.hpp"

#include <cstdint>
#include <utility>
#include <vector>

struct SSceneData {
    std::uint64_t                      tick = 0;
    std::vector<Game::SSnapshotEntity> entities;
    Ecs::CRegistry                     renderRegistry;

    SSceneData() = default;

    SSceneData(const SSceneData& other) : tick(other.tick), entities(other.entities) {
        rebuildRenderRegistry();
    }

    SSceneData& operator=(const SSceneData& other) {
        if (this != &other) {
            tick     = other.tick;
            entities = other.entities;
            rebuildRenderRegistry();
        }
        return *this;
    }

    SSceneData(SSceneData&&) noexcept            = default;
    SSceneData& operator=(SSceneData&&) noexcept = default;

    void        replaceEntities(std::vector<Game::SSnapshotEntity> nextEntities) {
        entities = std::move(nextEntities);
        rebuildRenderRegistry();
    }

  private:
    void rebuildRenderRegistry() {
        renderRegistry.clear();
        for (const auto& entity : entities) {
            const auto renderEntity = renderRegistry.create();
            renderRegistry.emplace<SCPosition>(renderEntity, SCPosition{entity.x, entity.y});
            renderRegistry.emplace<SCEntityType>(renderEntity, SCEntityType{entity.kind});
            renderRegistry.emplace<SCDrawable>(
                renderEntity,
                SCDrawable{eDrawableShape::CIRCLE,
                           entity.kind == Game::eEntityKind::PLAYER ? 18.0F : 8.0F,
                           static_cast<std::uint8_t>(entity.kind)});
            if (entity.kind == Game::eEntityKind::PLAYER || entity.kind == Game::eEntityKind::ENEMY) {
                renderRegistry.emplace<SCHealth>(renderEntity, SCHealth{entity.health});
            }
        }
    }
};
