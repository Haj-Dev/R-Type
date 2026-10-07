#pragma once

#include "Abstract/AComponent.hpp"

#include <cstdint>

struct SCHealth : AComponent {
    SCHealth() = default;
    explicit SCHealth(std::int16_t health) : value(health) {}

    std::int16_t value = 1;
};
