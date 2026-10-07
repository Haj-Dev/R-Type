#pragma once

#include "Abstract/AComponent.hpp"

#include <cstdint>

struct SCProjectile : AComponent {
    SCProjectile() = default;
    SCProjectile(std::uint8_t projectileOwner, float projectileDamage) :
        owner(projectileOwner), damage(projectileDamage) {}

    std::uint8_t owner  = 0;
    float        damage = 1.0F;
};
