#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

struct SCCollider : AComponent {
  public:
    SCCollider() = default;
    explicit SCCollider(float collisionRadius) : radius(collisionRadius) {}
    explicit SCCollider(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(radius) || !reader.finished())
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::COLLIDER;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, radius);
    }

    float radius = 8.0F;
};
