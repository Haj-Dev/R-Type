#pragma once

#pragma once

#include <array>
#include <cstdint>

struct SPlayerActionState {
    bool                up    = false;
    bool                down  = false;
    bool                left  = false;
    bool                right = false;
    bool                fire  = false;

    [[nodiscard]] float horizontal() const {
        return static_cast<float>(right) - static_cast<float>(left);
    }

    [[nodiscard]] float vertical() const {
        return static_cast<float>(down) - static_cast<float>(up);
    }
};

struct SPLayerActions {
    static constexpr std::size_t               MaxPlayers = 4;
    std::array<SPlayerActionState, MaxPlayers> states{};
    std::array<bool, MaxPlayers>               connected{};

    [[nodiscard]] const SPlayerActionState&    forPlayer(std::uint8_t id) const {
        static constexpr SPlayerActionState empty{};
        return id < MaxPlayers ? states.at(id) : empty;
    }
};
