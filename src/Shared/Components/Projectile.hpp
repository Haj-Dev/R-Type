#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

#include <cstdint>

struct SCProjectile : AComponent {
  public:
    SCProjectile() = default;
    SCProjectile(std::uint8_t projectileOwner, float projectileDamage) :
        owner(projectileOwner), damage(projectileDamage) {}
    explicit SCProjectile(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(owner) || !reader.read(damage) || !reader.finished())
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::PROJECTILE;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, owner);
        Components::append(data, damage);
    }

    std::uint8_t owner  = 0;
    float        damage = 1.0F;
};
