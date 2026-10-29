#pragma once

#include "ChatMessage.hpp"
#include "Collider.hpp"
#include "DrawableShape.hpp"
#include "DrawableSprite.hpp"
#include "DrawableSpriteAtlas.hpp"
#include "EntityType.hpp"
#include "Health.hpp"
#include "Player.hpp"
#include "Position.hpp"
#include "Projectile.hpp"
#include "Velocity.hpp"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace std {
    template <>
    struct hash<AComponent::eType> {
        std::size_t operator()(AComponent::eType type) const noexcept {
            return static_cast<std::size_t>(type);
        }
    };
} // namespace std

class CComponentRegistry {
  public:
    using Factory = std::function<std::unique_ptr<AComponent>(const std::vector<std::uint8_t>&)>;

    CComponentRegistry() {
        registerType<SCPosition>();
        registerType<SCVelocity>();
        registerType<SCHealth>();
        registerType<SCPlayer>();
        registerType<SCProjectile>();
        registerType<SCCollider>();
        registerType<SCDrawableShape>();
        registerType<SCDrawableSprite>();
        registerType<SCDrawableSpriteAtlas>();
        registerType<SCEntityType>();
        registerType<SCChatMessage>();
    }

    template <typename T>
    void registerType() {
        factories_[T{}.type()] = [](const std::vector<std::uint8_t>& data) {
            return std::make_unique<T>(data);
        };
    }

    [[nodiscard]] std::unique_ptr<AComponent> deserialize(const std::vector<std::uint8_t>& data) const {
        if (data.empty())
            return nullptr;
        const auto type = static_cast<AComponent::eType>(data.front());
        const auto it   = factories_.find(type);
        if (it == factories_.end())
            return nullptr;
        std::vector<std::uint8_t> payload(data.begin() + 1, data.end());
        auto                      component = it->second(payload);
        return component->valid() ? std::move(component) : nullptr;
    }

  private:
    std::unordered_map<AComponent::eType, Factory> factories_;
};
