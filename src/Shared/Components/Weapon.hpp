#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace Game {
    enum class eWeaponMode : std::uint8_t {
        STANDARD,
        HEAVY,
        SPREAD,
    };

    enum class ePowerUpKind : std::uint8_t {
        HEAVY,
        SPREAD,
    };
} // namespace Game

struct SCWeapon : AComponent {
    SCWeapon() = default;
    explicit SCWeapon(Game::eWeaponMode weaponMode, float duration = 0.0F) :
        mode(weaponMode), durationRemaining(duration) {}

    explicit SCWeapon(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(mode) || !reader.read(durationRemaining) || !reader.finished() ||
            (mode != Game::eWeaponMode::STANDARD && mode != Game::eWeaponMode::HEAVY &&
             mode != Game::eWeaponMode::SPREAD) ||
            !std::isfinite(durationRemaining) || durationRemaining < 0.0F ||
            (mode == Game::eWeaponMode::STANDARD && durationRemaining != 0.0F) ||
            (mode != Game::eWeaponMode::STANDARD && durationRemaining <= 0.0F))
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::WEAPON;
    }

    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, mode);
        Components::append(data, durationRemaining);
    }

    Game::eWeaponMode mode              = Game::eWeaponMode::STANDARD;
    float             durationRemaining = 0.0F;
};

struct SCPowerUp : AComponent {
    SCPowerUp() = default;
    explicit SCPowerUp(Game::ePowerUpKind powerUpKind) : kind(powerUpKind) {}

    explicit SCPowerUp(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(kind) || !reader.finished() ||
            (kind != Game::ePowerUpKind::HEAVY && kind != Game::ePowerUpKind::SPREAD))
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::POWER_UP;
    }

    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, kind);
    }

    Game::ePowerUpKind kind = Game::ePowerUpKind::HEAVY;
};
