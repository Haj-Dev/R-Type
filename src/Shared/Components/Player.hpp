#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

#include <cstdint>

struct SCPlayer : AComponent {
  public:
    SCPlayer() = default;
    explicit SCPlayer(std::uint8_t playerId) : id(playerId) {}
    explicit SCPlayer(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(id) || !reader.read(fireCooldown) || !reader.read(damageCooldown) ||
            !reader.finished())
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::PLAYER;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, id);
        Components::append(data, fireCooldown);
        Components::append(data, damageCooldown);
    }

    std::uint8_t id             = 0;
    float        fireCooldown   = 0.0F;
    float        damageCooldown = 0.0F;
};
