#pragma once

#include "Shared/Components/ComponentRegistry.hpp"
#include "Shared/ECS/ECS.hpp"
#include "Shared/GameWorld.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

struct SSceneData {
    std::uint64_t                        tick = 0;
    std::vector<Game::SEntityComponents> entities;
    Ecs::CRegistry                       renderRegistry;

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

    void        replaceEntities(std::vector<Game::SEntityComponents> nextEntities) {
        entities = std::move(nextEntities);
        rebuildRenderRegistry();
    }

    bool applyUpdates(const std::vector<Game::SUpdateEntity>& updates) {
        CComponentRegistry registry;
        auto               nextEntities = entities;
        for (const auto& update : updates) {
            if (!update.remove) {
                for (const auto& bytes : update.components) {
                    if (!registry.deserialize(bytes))
                        return false;
                }
            }
        }
        for (const auto& update : updates) {
            const auto it = std::ranges::find_if(
                nextEntities, [&update](const auto& entity) { return entity.id == update.id; });
            if (update.remove) {
                if (it != nextEntities.end())
                    nextEntities.erase(it);
                continue;
            }
            auto entity = it == nextEntities.end() ? Game::SEntityComponents{} : *it;
            if (it == nextEntities.end())
                continue;
            entity.id = update.id;
            entity.components.insert(entity.components.end(), update.components.begin(),
                                     update.components.end());
            if (it == nextEntities.end())
                nextEntities.push_back(std::move(entity));
        }
        replaceEntities(std::move(nextEntities));
        return true;
    }

  private:
    void rebuildRenderRegistry() {
        renderRegistry.clear();
        CComponentRegistry registry;
        for (const auto& entity : entities) {
            const auto renderEntity = renderRegistry.create();
            for (const auto& bytes : entity.components) {
                registry.apply(renderRegistry, renderEntity, bytes);
            }
        }
    }
};
