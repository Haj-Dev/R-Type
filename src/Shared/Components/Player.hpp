#pragma once

#include "Abstract/AComponent.hpp"

#include <cstdint>

struct SCPlayer : AComponent {
    SCPlayer() = default;
    explicit SCPlayer(std::uint8_t playerId) : id(playerId) {}

    std::uint8_t id           = 0;
    float        fireCooldown = 0.0F;
};
