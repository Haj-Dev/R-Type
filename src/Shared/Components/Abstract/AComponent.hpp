#pragma once

#include <cstdint>
#include <vector>

class AComponent {
  public:
    AComponent()          = default;
    virtual ~AComponent() = default;
    explicit AComponent([[maybe_unused]] std::vector<std::uint8_t> data) {}

    AComponent(const AComponent&)            = default;
    AComponent& operator=(const AComponent&) = default;
    AComponent(AComponent&&)                 = default;
    AComponent& operator=(AComponent&&)      = default;
};
