#pragma once

#include <cstdint>
#include <vector>

namespace Game {

    inline constexpr float WorldWidth      = 1280.0F;
    inline constexpr float WorldHeight     = 720.0F;
    inline constexpr float PlayerSpeed     = 260.0F;
    inline constexpr float ProjectileSpeed = 520.0F;
    inline constexpr float FireInterval    = 0.2F;

    struct SEntityComponents {
        std::uint32_t                          id           = 0;
        std::uint64_t                          creationTick = 0;
        std::vector<std::vector<std::uint8_t>> components;
    };

    struct SUpdateEntity {
        std::uint32_t                          id     = 0;
        bool                                   remove = false;
        std::vector<std::vector<std::uint8_t>> components;
    };

} // namespace Game
