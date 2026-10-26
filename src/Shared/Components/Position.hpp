#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

struct SCPosition : AComponent {
  public:
    SCPosition() = default;
    SCPosition(float xValue, float yValue) : x(xValue), y(yValue) {}
    explicit SCPosition(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(x) || !reader.read(y) || !reader.finished())
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::POSITION;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, x);
        Components::append(data, y);
    }

    float x = 0.0F;
    float y = 0.0F;
};
