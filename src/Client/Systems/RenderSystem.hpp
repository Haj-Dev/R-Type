#pragma once

#include "Shared/ECS/ECS.hpp"
#include "Shared/Components/Drawable.hpp"
#include "Shared/Components/EntityType.hpp"
#include "Shared/Components/Health.hpp"
#include "Shared/Components/Position.hpp"

#include <algorithm>
#include <cstdint>
#include <raylib.h>

class CRenderSystem {
  public:
    void draw(Ecs::CRegistry& registry) const {
        registry.each<SCPosition, SCDrawable>(
            [&](Ecs::SEntity entity, SCPosition& position, SCDrawable& drawable) {
                const auto* health = registry.get<SCHealth>(entity);
                if (health != nullptr && health->value <= 0)
                    return;

                DrawCircleV(Vector2{.x = position.x, .y = position.y}, drawable.radius,
                            color(drawable.color));

                if (drawable.color == static_cast<std::uint8_t>(Game::eEntityKind::PLAYER) &&
                    health != nullptr) {
                    constexpr float barWidth = 42.0F;
                    constexpr float barHeight = 6.0F;
                    constexpr float maxHealth = 3.0F;
                    const float healthRatio = static_cast<float>(std::clamp<int>(health->value, 0, 3)) / maxHealth;
                    const int barX = static_cast<int>(position.x - barWidth / 2.0F);
                    const int barY = static_cast<int>(position.y - drawable.radius - 14.0F);
                    DrawRectangle(barX, barY, static_cast<int>(barWidth), static_cast<int>(barHeight), DARKGRAY);
                    DrawRectangle(barX, barY, static_cast<int>(barWidth * healthRatio),
                                  static_cast<int>(barHeight), healthRatio > 0.5F ? GREEN : RED);
                    DrawText(TextFormat("PV: %d/3", static_cast<int>(health->value)),
                             barX - 2, barY - 17, 12, RAYWHITE);
                }
            });
    }

  private:
    static Color color(std::uint8_t value) {
        if (value == static_cast<std::uint8_t>(Game::eEntityKind::ENEMY))
            return RED;
        if (value == static_cast<std::uint8_t>(Game::eEntityKind::PLAYER_PROJECTILE))
            return YELLOW;
        return BLUE;
    }
};
