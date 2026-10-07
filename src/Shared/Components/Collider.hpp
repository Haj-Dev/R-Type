#pragma once

#include "Abstract/AComponent.hpp"

struct SCCollider : AComponent {
    SCCollider() = default;
    explicit SCCollider(float collisionRadius) : radius(collisionRadius) {}

    float radius = 8.0F;
};
