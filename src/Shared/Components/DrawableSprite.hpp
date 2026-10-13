#pragma once

#include "Abstract/AComponent.hpp"
#include "Position.hpp"

#include <string>
#include <utility>

struct SCDrawableSprite : AComponent {
    SCDrawableSprite() = default;
    explicit SCDrawableSprite(std::string drawableTexture, int drawableLayer = 0) :
        texture(std::move(drawableTexture)), layer(drawableLayer) {}

    std::string texture;
    int         layer   = 0;
    float       scale   = 1.0F;
    SCPosition  offset{};
    float       rotation = 0.0F;
};
