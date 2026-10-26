#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

#include <cstdint>

namespace Game {

    enum class eEntityKind : std::uint8_t {
        PLAYER,
        ENEMY,
        PLAYER_PROJECTILE,
        ENEMY_PROJECTILE,
    };

} // namespace Game

struct SCEntityType : AComponent {
    SCEntityType() = default;
    explicit SCEntityType(Game::eEntityKind entityKind) : value(entityKind) {}
    explicit SCEntityType(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(value) || !reader.finished())
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::ENTITY_TYPE;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, value);
    }

    Game::eEntityKind value = Game::eEntityKind::ENEMY;
};
