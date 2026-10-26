#pragma once

#include "Abstract/AComponent.hpp"
#include "Position.hpp"
#include "Serialization.hpp"

#include <string>
#include <utility>

struct SCDrawableSprite : AComponent {
  public:
    SCDrawableSprite() = default;
    explicit SCDrawableSprite(std::string drawableTexture, std::int8_t drawableLayer = 0,
                              float drawableScale = 1.0F, SCPosition drawableOffset = {},
                              float drawableRotation = 0.0F) :
        texture(std::move(drawableTexture)), layer(drawableLayer), scale(drawableScale),
        offset(std::move(drawableOffset)), rotation(drawableRotation) {}
    explicit SCDrawableSprite(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.readString(texture) || !reader.read(layer) || !reader.read(scale) ||
            !reader.read(offset.x) || !reader.read(offset.y) || !reader.read(rotation) ||
            !reader.finished())
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::DRAWABLE_SPRITE;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::appendString(data, texture);
        Components::append(data, layer);
        Components::append(data, scale);
        Components::append(data, offset.x);
        Components::append(data, offset.y);
        Components::append(data, rotation);
    }

    std::string texture;
    std::int8_t layer = 0;
    float       scale = 1.0F;
    SCPosition  offset{};
    float       rotation = 0.0F;
};
