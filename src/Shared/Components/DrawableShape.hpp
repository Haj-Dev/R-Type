#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

enum class eDrawableShape : std::uint8_t {
    CIRCLE,
};

struct SCDrawableShape : AComponent {
  public:
    SCDrawableShape() = default;
    SCDrawableShape(eDrawableShape drawableShape, float drawableRadius, std::uint8_t drawableColor,
                    std::int8_t drawableLayer = 0) :
        shape(drawableShape), radius(drawableRadius), color(drawableColor), layer(drawableLayer) {}
    explicit SCDrawableShape(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(shape) || !reader.read(radius) || !reader.read(color) || !reader.read(layer) ||
            !reader.finished())
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::DRAWABLE_SHAPE;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, shape);
        Components::append(data, radius);
        Components::append(data, color);
        Components::append(data, layer);
    }

    eDrawableShape shape  = eDrawableShape::CIRCLE;
    float          radius = 8.0F;
    std::uint8_t   color  = 0;
    std::int8_t    layer  = 0;
};
