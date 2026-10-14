#pragma once

#include "Shared/Systems/RenderingSystem.hpp"

#include <algorithm>
#include <raylib.h>

class CRenderSystem {
  public:
    void loadTextures(const Ecs::CRegistry& registry) {
        renderer_.loadTextures(registry);
    }

    void draw(Ecs::CRegistry& registry) const {
        renderer_.draw(registry);
        registry.each<SCPosition, SCEntityType, SCHealth>(
            [&](Ecs::SEntity entity, SCPosition& position, SCEntityType& type, SCHealth& health) {
                if (type.value == Game::eEntityKind::PLAYER) {
                    constexpr float barWidth  = 42.0F;
                    constexpr float barHeight = 6.0F;
                    constexpr float maxHealth = 3.0F;
                    const float     healthRatio =
                        static_cast<float>(std::clamp<int>(health.value, 0, 3)) / maxHealth;
                    const auto* shape  = registry.get<SCDrawableShape>(entity);
                    const float radius = shape != nullptr ? shape->radius : 18.0F;
                    const int   barX   = static_cast<int>(position.x - (barWidth / 2.0F));
                    const int   barY   = static_cast<int>(position.y - radius - 14.0F);
                    DrawRectangle(barX, barY, static_cast<int>(barWidth), static_cast<int>(barHeight),
                                  DARKGRAY);
                    DrawRectangle(barX, barY, static_cast<int>(barWidth * healthRatio),
                                  static_cast<int>(barHeight), healthRatio > 0.5F ? GREEN : RED);
                    DrawText(TextFormat("HP: %d/3", static_cast<int>(health.value)), barX - 2, barY - 17,
                             12, RAYWHITE);
                }
            });
    }

  private:
    Game::CRenderingSystem renderer_;
};
