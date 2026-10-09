#pragma once

#include "Abstract/AComponent.hpp"

struct SCPosition : AComponent {
    SCPosition() = default;
    SCPosition(float xValue, float yValue) : x(xValue), y(yValue) {}

    float x = 0.0F;
    float y = 0.0F;
};
