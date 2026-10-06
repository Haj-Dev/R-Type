#pragma once

#include "Abstract/AComponent.hpp"

#include "raylib.h"

class CPosition : public AComponent {
  public:
    explicit CPosition(Vector2 position = Vector2{.x = 0.0f, .y = 0.0f}) : position(position) {}
    virtual ~CPosition() = default;
    explicit CPosition(std::vector<uint8_t> data) {
        // Deserialize the data to initialize the component
    }

    void onTick([[maybe_unused]] const size_t ID, [[maybe_unused]] const float delta) override {

        // nothing
    }

    void onDraw([[maybe_unused]] const size_t ID) override {
        // nothing
    }

    std::vector<uint8_t> onNetworkSend(const size_t ID) override {
        // Send position data over the network here
    }

  private:
    Vector2 position;
};