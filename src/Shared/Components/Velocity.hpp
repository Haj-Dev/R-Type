#pragma once

#include "Abstract/AComponent.hpp"

struct SCVelocity : AComponent {
    SCVelocity() = default;
    SCVelocity(float xValue, float yValue) : x(xValue), y(yValue) {}

    float x = 0.0F;
    float y = 0.0F;
};
