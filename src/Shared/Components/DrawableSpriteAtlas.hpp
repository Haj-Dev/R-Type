#pragma once

#include "DrawableSprite.hpp"
#include "Shared/Dimentions.hpp"
#include "Serialization.hpp"

#include <cstddef>
#include <cmath>
#include <algorithm>
#include <utility>

struct SCDrawableSpriteAtlas : SCDrawableSprite {
  public:
    SCDrawableSpriteAtlas() = default;
    explicit SCDrawableSpriteAtlas(std::string drawableTexture, SDimentions drawableBaseDimension,
                                   std::size_t drawableFrames = 1U, float drawableFrameDuration = 0.0F,
                                   std::size_t drawableVariant = 0U, std::int8_t drawableLayer = 0,
                                   float drawableScale = 1.0F, SCPosition drawableOffset = {},
                                   float drawableRotation = 0.0F) :
        SCDrawableSprite(std::move(drawableTexture), drawableLayer, drawableScale,
                         std::move(drawableOffset), drawableRotation),
        baseDimension(drawableBaseDimension), frames(drawableFrames),
        frameDuration(drawableFrameDuration), variant(drawableVariant) {}
    explicit SCDrawableSpriteAtlas(const std::vector<std::uint8_t>& data) : SCDrawableSprite() {
        Components::CReader reader(data);
        if (!reader.readString(texture) || !reader.read(layer) || !reader.read(scale) ||
            !reader.read(offset.x) || !reader.read(offset.y) || !reader.read(rotation) ||
            !reader.read(baseDimension.width) || !reader.read(baseDimension.height) ||
            !reader.read(frames) || !reader.read(frameDuration) || !reader.read(variant) ||
            !reader.finished())
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::DRAWABLE_SPRITE_ATLAS;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::appendString(data, texture);
        Components::append(data, layer);
        Components::append(data, scale);
        Components::append(data, offset.x);
        Components::append(data, offset.y);
        Components::append(data, rotation);
        Components::append(data, baseDimension.width);
        Components::append(data, baseDimension.height);
        Components::append(data, frames);
        Components::append(data, frameDuration);
        Components::append(data, variant);
    }

    SDimentions baseDimension{};
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
