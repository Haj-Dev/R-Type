#pragma once

#include "DrawableSprite.hpp"

#include <cstddef>
#include <cmath>
#include <algorithm>
#include <utility>

#include <raylib.h>

struct SCDrawableSpriteAtlas : SCDrawableSprite {
    SCDrawableSpriteAtlas() = default;
    explicit SCDrawableSpriteAtlas(std::string drawableTexture, Rectangle drawableBaseDimension,
                                   std::size_t drawableFrames = 1U, float drawableFrameDuration = 0.0F,
                                   std::size_t drawableVariant = 0U, int drawableLayer = 0,
                                   float drawableScale = 1.0F, SCPosition drawableOffset = {},
                                   float drawableRotation = 0.0F) :
        SCDrawableSprite(std::move(drawableTexture), drawableLayer, drawableScale,
                         std::move(drawableOffset), drawableRotation),
        baseDimension(drawableBaseDimension), frames(drawableFrames),
        frameDuration(drawableFrameDuration), variant(drawableVariant) {}

    Rectangle   baseDimension{};
    std::size_t frames        = 1U;
    float       frameDuration = 0.0F;
    std::size_t variant       = 0U;

    void        advance(float delta) {
        if (frames <= 1U || frameDuration <= 0.0F)
            return;
        const float elapsed       = currentTime + std::max(0.0F, delta);
        const auto  elapsedFrames = static_cast<std::size_t>((elapsed + 1.0e-6F) / frameDuration);
        currentFrameIndex         = (currentFrameIndex + elapsedFrames) % frames;
        currentTime               = std::fmod(elapsed, frameDuration);
    }

    [[nodiscard]] std::size_t currentFrame() const {
        return frames == 0U ? 0U : std::min(currentFrameIndex, frames - 1U);
    }

    [[nodiscard]] float animationTime() const {
        return currentTime;
    }

    void restoreAnimationState(std::size_t frame, float time) {
        currentFrameIndex = frame;
        currentTime       = time;
    }

  private:
    float       currentTime       = 0.0F;
    std::size_t currentFrameIndex = 0U;
};
