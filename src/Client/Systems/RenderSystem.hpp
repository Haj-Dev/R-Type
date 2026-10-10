#pragma once

#include "Shared/ECS/ECS.hpp"
#include "Shared/Components/Drawable.hpp"
#include "Shared/Components/EntityType.hpp"
#include "Shared/Components/Health.hpp"
#include "Shared/Components/Position.hpp"

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
