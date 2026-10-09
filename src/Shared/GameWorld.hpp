#pragma once

#include "Shared/Components/EntityType.hpp"

#include <cstdint>

namespace Game {

    inline constexpr float WorldWidth      = 1280.0F;
    inline constexpr float WorldHeight     = 720.0F;
    inline constexpr float PlayerSpeed     = 260.0F;
    inline constexpr float ProjectileSpeed = 520.0F;
    inline constexpr float FireInterval    = 0.2F;

    struct SSnapshotEntity {
        std::uint32_t id     = 0;
        eEntityKind   kind   = eEntityKind::ENEMY;
        float         x      = 0.0F;
        float         y      = 0.0F;
        std::int16_t  health = 0;
    };

} // namespace Game
