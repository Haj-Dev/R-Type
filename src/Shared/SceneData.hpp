#pragma once

#include "Shared/ECS/ECS.hpp"
#include "Shared/Components/DrawableShape.hpp"
#include "Shared/Components/DrawableSprite.hpp"
#include "Shared/Components/DrawableSpriteAtlas.hpp"
#include "Shared/Components/ComponentRegistry.hpp"
#include "Shared/Components/EntityType.hpp"
#include "Shared/Components/Health.hpp"
#include "Shared/Components/Position.hpp"
#include "Shared/GameWorld.hpp"

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <unordered_map>
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
        const auto animationStates = captureAnimationStates();
        entities                   = std::move(nextEntities);
        rebuildRenderRegistry(animationStates);
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
            auto entity = it == nextEntities.end() ? Game::SSnapshotEntity{} : *it;
            entity.id   = update.id;
            for (const auto& bytes : update.components)
                applyComponent(entity, *registry.deserialize(bytes));
            if (it == nextEntities.end())
                nextEntities.push_back(std::move(entity));
            else
                *it = std::move(entity);
        }
        replaceEntities(std::move(nextEntities));
        return true;
    }

  private:
    using SAnimationState = std::pair<std::size_t, float>;

    static void applyComponent(Game::SSnapshotEntity& entity, const AComponent& component) {
        if (const auto* position = dynamic_cast<const SCPosition*>(&component)) {
            entity.x = position->x;
            entity.y = position->y;
        } else if (const auto* type = dynamic_cast<const SCEntityType*>(&component)) {
            entity.kind = type->value;
        } else if (const auto* health = dynamic_cast<const SCHealth*>(&component)) {
            entity.health = health->value;
        } else if (const auto* shape = dynamic_cast<const SCDrawableShape*>(&component)) {
            entity.texture.clear();
            entity.atlas = false;
            entity.layer = shape->layer;
        } else if (const auto* atlas = dynamic_cast<const SCDrawableSpriteAtlas*>(&component)) {
            entity.texture       = atlas->texture;
            entity.layer         = atlas->layer;
            entity.scale         = atlas->scale;
            entity.offsetX       = atlas->offset.x;
            entity.offsetY       = atlas->offset.y;
            entity.rotation      = atlas->rotation;
            entity.atlas         = true;
            entity.baseDimension = atlas->baseDimension;
            entity.frames        = atlas->frames;
            entity.frameDuration = atlas->frameDuration;
            entity.variant       = atlas->variant;
        } else if (const auto* sprite = dynamic_cast<const SCDrawableSprite*>(&component)) {
            entity.texture  = sprite->texture;
            entity.layer    = sprite->layer;
            entity.scale    = sprite->scale;
            entity.offsetX  = sprite->offset.x;
            entity.offsetY  = sprite->offset.y;
            entity.rotation = sprite->rotation;
            entity.atlas    = false;
        }
    }

    [[nodiscard]] std::unordered_map<std::uint32_t, SAnimationState> captureAnimationStates() const {
        std::unordered_map<std::uint32_t, SAnimationState> states;
        std::vector<Ecs::SEntity>                          renderEntities;
        renderRegistry.eachEntity(
            [&renderEntities](Ecs::SEntity entity) { renderEntities.push_back(entity); });
        const auto count = std::min(entities.size(), renderEntities.size());
        for (std::size_t index = 0; index < count; ++index) {
            if (const auto* atlas = renderRegistry.get<SCDrawableSpriteAtlas>(renderEntities.at(index)))
                states.emplace(entities.at(index).id,
                               SAnimationState{atlas->currentFrame(), atlas->animationTime()});
        }
        return states;
    }

    void rebuildRenderRegistry(
        const std::unordered_map<std::uint32_t, SAnimationState>& animationStates = {}) {
        renderRegistry.clear();
        for (const auto& entity : entities) {
            const auto renderEntity = renderRegistry.create();
            renderRegistry.emplace<SCPosition>(renderEntity, SCPosition{entity.x, entity.y});
            renderRegistry.emplace<SCEntityType>(renderEntity, SCEntityType{entity.kind});
            if (entity.texture.empty()) {
                renderRegistry.emplace<SCDrawableShape>(
                    renderEntity,
                    SCDrawableShape{eDrawableShape::CIRCLE,
                                    entity.kind == Game::eEntityKind::PLAYER ? 18.0F : 8.0F,
                                    static_cast<std::uint8_t>(entity.kind), entity.layer});
            } else {
                if (entity.atlas) {
                    renderRegistry.emplace<SCDrawableSpriteAtlas>(
                        renderEntity,
                        SCDrawableSpriteAtlas{entity.texture, entity.baseDimension, entity.frames,
                                              entity.frameDuration, entity.variant, entity.layer});
                } else {
                    renderRegistry.emplace<SCDrawableSprite>(
                        renderEntity, SCDrawableSprite{entity.texture, entity.layer});
                }
                auto* sprite = renderRegistry.get<SCDrawableSprite>(renderEntity);
                if (auto* atlas = renderRegistry.get<SCDrawableSpriteAtlas>(renderEntity))
                    sprite = static_cast<SCDrawableSprite*>(atlas);
                sprite->scale    = entity.scale;
                sprite->offset   = SCPosition{entity.offsetX, entity.offsetY};
                sprite->rotation = entity.rotation;
                if (auto* atlas = renderRegistry.get<SCDrawableSpriteAtlas>(renderEntity)) {
                    if (const auto state = animationStates.find(entity.id);
                        state != animationStates.end())
                        atlas->restoreAnimationState(state->second.first, state->second.second);
                }
            }
            if (entity.kind == Game::eEntityKind::PLAYER || entity.kind == Game::eEntityKind::ENEMY) {
                renderRegistry.emplace<SCHealth>(renderEntity, SCHealth{entity.health});
            }
        }
    }
};
