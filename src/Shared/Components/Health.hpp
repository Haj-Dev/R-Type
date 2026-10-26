#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

#include <cstdint>

struct SCHealth : AComponent {
  public:
    SCHealth() = default;
    explicit SCHealth(std::int16_t health) : value(health) {}
    explicit SCHealth(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(value) || !reader.finished())
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::HEALTH;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, value);
    }

    std::int16_t value = 1;
};
