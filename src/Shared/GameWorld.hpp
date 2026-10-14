#pragma once

#include "Shared/Components/EntityType.hpp"

#include <cstdint>
#include <cstddef>
#include <string>

#include <raylib.h>

namespace Game {

    inline constexpr float WorldWidth      = 1280.0F;
    inline constexpr float WorldHeight     = 720.0F;
    inline constexpr float PlayerSpeed     = 260.0F;
    inline constexpr float ProjectileSpeed = 520.0F;
    inline constexpr float FireInterval    = 0.2F;

    struct SSnapshotEntity {
        std::uint32_t id       = 0;
        eEntityKind   kind     = eEntityKind::ENEMY;
        float         x        = 0.0F;
        float         y        = 0.0F;
        std::int16_t  health   = 0;
        int           layer    = 0;
        float         scale    = 1.0F;
        float         offsetX  = 0.0F;
        float         offsetY  = 0.0F;
        float         rotation = 0.0F;
        std::string   texture;
        bool          atlas = false;
        Rectangle     baseDimension{};
        std::size_t   frames        = 1U;
        float         frameDuration = 0.0F;
        std::size_t   variant       = 0U;
    };

} // namespace Game
