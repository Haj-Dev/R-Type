#pragma once

#include "Abstract/AComponent.hpp"

#include "raylib.h"

class CDrawable : public AComponent {
  public:
    explicit CDrawable(/* image/texture ref, */ Vector2 offset = Vector2{.x = 0.0f, .y = 0.0f},
                       int                              layer  = 0) : offset(offset), layer(layer) {}
    virtual ~CDrawable() = default;
    explicit CDrawable(std::vector<uint8_t> data) {
        // Deserialize the data to initialize the component
    }

    void onTick([[maybe_unused]] const size_t ID, [[maybe_unused]] const float delta) override {
        // nothing
    }

    void onDraw(const size_t ID) override {
        //Vector2 position = from something;
        //something queueDrawTexture
    }

    std::vector<uint8_t> onNetworkSend(const size_t ID) override {
        // Send position data over the network here
    }

  private:
    Vector2 offset;
    int     layer;
};