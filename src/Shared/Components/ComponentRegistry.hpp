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
#include "Shared/ECS/ECS.hpp"

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
    using Apply   = std::function<void(Ecs::CRegistry&, Ecs::SEntity, const AComponent&)>;
    using Serialize =
        std::function<void(const Ecs::CRegistry&, Ecs::SEntity, std::vector<std::uint8_t>&)>;

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
        const auto type  = T{}.type();
        factories_[type] = [](const std::vector<std::uint8_t>& data) {
            return std::make_unique<T>(data);
        };
        apply_[type] = [](Ecs::CRegistry& registry, Ecs::SEntity entity, const AComponent& component) {
            registry.emplace<T>(entity, static_cast<const T&>(component));
        };
        serializers_[type] = [](const Ecs::CRegistry& registry, Ecs::SEntity entity,
                                std::vector<std::uint8_t>& data) {
            if (const auto* component = registry.get<T>(entity))
                component->serialize(data);
        };
        order_.push_back(type);
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

    bool apply(Ecs::CRegistry& registry, Ecs::SEntity entity,
               const std::vector<std::uint8_t>& data) const {
        if (data.empty())
            return false;
        const auto type      = static_cast<AComponent::eType>(data.front());
        const auto it        = apply_.find(type);
        auto       component = deserialize(data);
        if (it == apply_.end() || component == nullptr)
            return false;
        it->second(registry, entity, *component);
        return true;
    }

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> serialize(const Ecs::CRegistry& registry,
                                                                   Ecs::SEntity          entity) const {
        std::vector<std::vector<std::uint8_t>> result;
        for (const auto type : order_) {
            std::vector<std::uint8_t> bytes;
            serializers_.at(type)(registry, entity, bytes);
            if (!bytes.empty())
                result.push_back(std::move(bytes));
        }
        return result;
    }

  private:
    std::unordered_map<AComponent::eType, Factory>   factories_;
    std::unordered_map<AComponent::eType, Apply>     apply_;
    std::unordered_map<AComponent::eType, Serialize> serializers_;
    std::vector<AComponent::eType>                   order_;
};
