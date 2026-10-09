#pragma once

#include "Abstract/AComponent.hpp"

#include <cstdint>

namespace Game {

    enum class eEntityKind : std::uint8_t {
        PLAYER,
        ENEMY,
        PLAYER_PROJECTILE,
        ENEMY_PROJECTILE,
    };

} // namespace Game

struct SCEntityType : AComponent {
    SCEntityType() = default;
    explicit SCEntityType(Game::eEntityKind entityKind) : value(entityKind) {}

    Game::eEntityKind value = Game::eEntityKind::ENEMY;
};
