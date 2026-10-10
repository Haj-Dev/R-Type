#pragma once

#include "Abstract/AComponent.hpp"

enum class eDrawableShape : std::uint8_t {
    CIRCLE,
};

struct SCDrawable : AComponent {
    SCDrawable() = default;
    SCDrawable(eDrawableShape drawableShape, float drawableRadius, std::uint8_t drawableColor,
               int drawableLayer = 0) :
        shape(drawableShape), radius(drawableRadius), color(drawableColor), layer(drawableLayer) {}

    eDrawableShape shape  = eDrawableShape::CIRCLE;
    float          radius = 8.0F;
    std::uint8_t   color  = 0;
    int            layer  = 0;
};
