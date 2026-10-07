#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class AComponent {
  public:
    AComponent()          = default;
    virtual ~AComponent() = default;
    explicit AComponent(std::vector<uint8_t> data);

    virtual void                 onTick(const size_t ID, const float delta) = 0;
    virtual void                 onDraw(const size_t ID)                    = 0;
    virtual std::vector<uint8_t> onNetworkSend(const size_t ID)             = 0;
};