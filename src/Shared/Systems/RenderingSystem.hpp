#pragma once

#include "Shared/Components/DrawableShape.hpp"
#include "Shared/Components/DrawableSprite.hpp"
#include "Shared/Components/DrawableSpriteAtlas.hpp"
#include "Shared/Components/Health.hpp"
#include "Shared/Components/EntityType.hpp"
#include "Shared/Components/Position.hpp"
#include "Shared/ECS/ECS.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <raylib.h>

namespace Game {

    class CRenderingSystem {
      public:
        ~CRenderingSystem() {
            for (auto& [path, texture] : textures_)
                UnloadTexture(texture);
        }

        void loadTextures(const Ecs::CRegistry& registry) {
            registry.eachEntity([this, &registry](Ecs::SEntity entity) {
                const auto* sprite = registry.get<SCDrawableSprite>(entity);
                const auto* atlas  = registry.get<SCDrawableSpriteAtlas>(entity);
                if (atlas != nullptr)
                    sprite = atlas;
                if (sprite != nullptr && !sprite->texture.empty())
                    loadTexture(sprite->texture);
            });
        }

        void draw(const Ecs::CRegistry& registry) const {
            struct SDrawCommand {
                int          layer;
                Ecs::SEntity entity;
                bool         sprite;
            };
            std::vector<SDrawCommand> commands;
            registry.eachEntity([&](Ecs::SEntity entity) {
                if (registry.get<SCPosition>(entity) == nullptr || !visible(registry, entity))
                    return;
                if (const auto* shape = registry.get<SCDrawableShape>(entity))
                    commands.push_back({.layer = shape->layer, .entity = entity, .sprite = false});
                if (const auto* sprite = registry.get<SCDrawableSprite>(entity))
                    commands.push_back({.layer = sprite->layer, .entity = entity, .sprite = true});
                if (const auto* atlas = registry.get<SCDrawableSpriteAtlas>(entity))
                    commands.push_back({.layer = atlas->layer, .entity = entity, .sprite = true});
            });
            std::ranges::stable_sort(
                commands, [](const auto& left, const auto& right) { return left.layer < right.layer; });
            for (const auto& command : commands) {
                const auto& position = *registry.get<SCPosition>(command.entity);
                if (command.sprite) {
                    const auto* sprite = registry.get<SCDrawableSprite>(command.entity);
                    if (const auto* atlas = registry.get<SCDrawableSpriteAtlas>(command.entity))
                        sprite = atlas;
                    if (sprite == nullptr)
                        continue;
                    const auto texture = textures_.find(sprite->texture);
                    if (texture == textures_.end())
                        continue;
                    const auto& image = texture->second;
                    Rectangle   source{
                          .x      = 0.0F,
                          .y      = 0.0F,
                          .width  = static_cast<float>(image.width),
                          .height = static_cast<float>(image.height),
                    };
                    if (const auto* atlas = registry.get<SCDrawableSpriteAtlas>(command.entity)) {
                        const auto frame = atlas->currentFrame();
                        source           = Rectangle{
                                      .x      = atlas->baseDimension.width * static_cast<float>(frame),
                                      .y      = atlas->baseDimension.height * static_cast<float>(atlas->variant),
                                      .width  = static_cast<float>(atlas->baseDimension.width),
                                      .height = static_cast<float>(atlas->baseDimension.height),
                        };
                        if (source.width <= 0.0F || source.height <= 0.0F || source.x < 0.0F ||
                            source.y < 0.0F ||
                            source.x + source.width > static_cast<float>(image.width) ||
                            source.y + source.height > static_cast<float>(image.height)) {
                            TraceLog(LOG_ERROR, "Skipping invalid sprite atlas rectangle for '%s'",
                                     sprite->texture.c_str());
                            continue;
                        }
                    }
                    const Rectangle destination{
                        .x      = position.x + sprite->offset.x,
                        .y      = position.y + sprite->offset.y,
                        .width  = source.width * sprite->scale,
                        .height = source.height * sprite->scale,
                    };
                    DrawTexturePro(
                        image, source, destination,
                        Vector2{.x = destination.width / 2.0F, .y = destination.height / 2.0F},
                        sprite->rotation, WHITE);
                } else {
                    const auto& shape = *registry.get<SCDrawableShape>(command.entity);
                    DrawCircleV(Vector2{.x = position.x, .y = position.y}, shape.radius,
                                color(shape.color));
                }
            }
        }

      private:
        void loadTexture(const std::string& path) {
            if (textures_.contains(path) || failedTextures_.contains(path))
                return;
            const auto texture = LoadTexture(path.c_str());
            if (texture.id == 0 || texture.width <= 0 || texture.height <= 0) {
                TraceLog(LOG_ERROR, "Failed to load texture '%s'", path.c_str());
                failedTextures_.insert(path);
                return;
            }
            textures_.emplace(path, texture);
        }

        static bool visible(const Ecs::CRegistry& registry, Ecs::SEntity entity) {
            const auto* health = registry.get<SCHealth>(entity);
            return health == nullptr || health->value > 0;
        }

        static Color color(std::uint8_t value) {
            if (value == static_cast<std::uint8_t>(Game::eEntityKind::ENEMY))
                return RED;
            if (value == static_cast<std::uint8_t>(Game::eEntityKind::PLAYER_PROJECTILE))
                return YELLOW;
            return BLUE;
        }

        std::unordered_map<std::string, Texture2D> textures_;
        std::unordered_set<std::string>            failedTextures_;
    };

} // namespace Game